#include "ttsengine.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

TTSEngine::TTSEngine(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audio);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState)
            emit playbackFinished();
    });
}

TTSEngine::~TTSEngine() {
    stop();
    delete m_tempFile;
}

QStringList TTSEngine::voices() {
    return {"mimo_default", "冰糖", "茉莉", "苏打", "白桦", "Mia", "Chloe", "Milo", "Dean"};
}

QStringList TTSEngine::defaultModels() {
    return {"mimo-v2.5-tts", "mimo-v2.5-tts-voicedesign", "mimo-v2.5-tts-voiceclone"};
}

QStringList TTSEngine::formats() {
    return {"wav", "pcm16"};
}

void TTSEngine::fetchModels(const QString &apiBase, const QString &apiKey) {
    if (apiKey.isEmpty()) {
        emit modelsError("API Key 不能为空");
        return;
    }

    QString base = apiBase;
    if (base.endsWith('/')) base.chop(1);
    QUrl url(base + "/models");

    QNetworkRequest req(url);
    req.setRawHeader("api-key", apiKey.toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit modelsError(QString("获取模型列表失败: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QStringList models;

        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto &item : arr) {
                QJsonObject obj = item.toObject();
                QString id = obj["id"].toString();
                if (!id.isEmpty())
                    models.append(id);
            }
        } else if (doc.isObject()) {
            QJsonArray arr = doc.object()["data"].toArray();
            for (const auto &item : arr) {
                QJsonObject obj = item.toObject();
                QString id = obj["id"].toString();
                if (!id.isEmpty())
                    models.append(id);
            }
        }

        if (models.isEmpty())
            models = defaultModels();

        emit modelsReceived(models);
    });
}

void TTSEngine::synthesize(const QString &text, const QString &apiBase, const QString &apiKey,
                           const QString &model, const QString &voice, const QString &format,
                           const QString &stylePrompt) {
    if (apiKey.isEmpty()) {
        emit synthesisError("API Key 不能为空");
        return;
    }
    if (text.trimmed().isEmpty()) {
        emit synthesisError("合成文本不能为空");
        return;
    }

    QString base = apiBase;
    if (base.endsWith('/')) base.chop(1);
    QUrl url(base + "/chat/completions");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("api-key", apiKey.toUtf8());

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = stylePrompt.isEmpty() ? "" : stylePrompt;
    messages.append(userMsg);

    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = text;
    messages.append(assistantMsg);

    QJsonObject audioCfg;
    audioCfg["format"] = format;
    if (model != "mimo-v2.5-tts-voicedesign")
        audioCfg["voice"] = voice;

    QJsonObject payload;
    payload["model"] = model;
    payload["messages"] = messages;
    payload["audio"] = audioCfg;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, format]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit synthesisError(QString("API 请求失败: %1 - %2")
                .arg(reply->error())
                .arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonArray choices = root["choices"].toArray();
        if (choices.isEmpty()) {
            emit synthesisError("返回数据格式错误");
            return;
        }

        QString audioData = choices[0].toObject()["message"].toObject()["audio"].toObject()["data"].toString();
        if (audioData.isEmpty()) {
            emit synthesisError("返回数据中没有音频内容");
            return;
        }

        QByteArray audioBytes = QByteArray::fromBase64(audioData.toUtf8());

        delete m_tempFile;
        m_tempFile = new QTemporaryFile(QDir::tempPath() + "/mimo_tts_XXXXXX." + (format == "wav" ? "wav" : "pcm"));
        if (!m_tempFile->open()) {
            emit synthesisError("无法创建临时文件");
            return;
        }
        m_tempFile->write(audioBytes);
        m_tempFile->flush();

        emit synthesisFinished(m_tempFile->fileName());
    });
}

void TTSEngine::cloneVoice(const QString &text, const QString &audioFilePath,
                           const QString &apiBase, const QString &apiKey, const QString &format) {
    if (apiKey.isEmpty()) {
        emit synthesisError("API Key 不能为空");
        return;
    }
    if (text.trimmed().isEmpty()) {
        emit synthesisError("合成文本不能为空");
        return;
    }
    if (audioFilePath.isEmpty() || !QFile::exists(audioFilePath)) {
        emit synthesisError("请选择参考音频文件");
        return;
    }

    QFile audioFile(audioFilePath);
    if (!audioFile.open(QIODevice::ReadOnly)) {
        emit synthesisError("无法读取音频文件");
        return;
    }
    QByteArray audioData = audioFile.readAll();
    audioFile.close();

    QString mimeType = "audio/wav";
    if (audioFilePath.endsWith(".mp3", Qt::CaseInsensitive))
        mimeType = "audio/mpeg";

    QString base64Audio = "data:" + mimeType + ";base64," + audioData.toBase64();

    QString base = apiBase;
    if (base.endsWith('/')) base.chop(1);
    QUrl url(base + "/chat/completions");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("api-key", apiKey.toUtf8());

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = "";
    messages.append(userMsg);

    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = text;
    messages.append(assistantMsg);

    QJsonObject audioCfg;
    audioCfg["format"] = format;
    audioCfg["voice"] = base64Audio;

    QJsonObject payload;
    payload["model"] = "mimo-v2.5-tts-voiceclone";
    payload["messages"] = messages;
    payload["audio"] = audioCfg;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, format]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit synthesisError(QString("API 请求失败: %1 - %2")
                .arg(reply->error())
                .arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonArray choices = root["choices"].toArray();
        if (choices.isEmpty()) {
            emit synthesisError("返回数据格式错误");
            return;
        }

        QString audioData = choices[0].toObject()["message"].toObject()["audio"].toObject()["data"].toString();
        if (audioData.isEmpty()) {
            emit synthesisError("返回数据中没有音频内容");
            return;
        }

        QByteArray audioBytes = QByteArray::fromBase64(audioData.toUtf8());

        delete m_tempFile;
        m_tempFile = new QTemporaryFile(QDir::tempPath() + "/mimo_clone_XXXXXX." + (format == "wav" ? "wav" : "pcm"));
        if (!m_tempFile->open()) {
            emit synthesisError("无法创建临时文件");
            return;
        }
        m_tempFile->write(audioBytes);
        m_tempFile->flush();

        emit synthesisFinished(m_tempFile->fileName());
    });
}

void TTSEngine::play(const QString &filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        emit synthesisError("音频文件不存在");
        return;
    }
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
}

void TTSEngine::stop() {
    m_player->stop();
}

bool TTSEngine::isPlaying() const {
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}
