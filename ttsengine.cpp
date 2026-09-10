#include "ttsengine.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDataStream>

namespace {

// 本地服务通常也接受 Bearer 头，但为空时干脆不发，避免个别服务把空头当成鉴权失败。
void applyAuthHeaders(QNetworkRequest &req, const QString &apiKey) {
    if (apiKey.isEmpty())
        return;
    const QByteArray key = apiKey.toUtf8();
    req.setRawHeader("Authorization", "Bearer " + key);
    // MiMo 用的是 api-key 头，一并带上以便两套协议都能工作。
    req.setRawHeader("api-key", key);
}

QString normalizedBase(const QString &apiBase) {
    QString base = apiBase.trimmed();
    while (base.endsWith('/'))
        base.chop(1);
    return base;
}

// 把服务端返回的 JSON 错误体压成一行可读文案。
QString extractApiError(const QByteArray &body) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QJsonObject error = obj.value("error").toObject();
        if (!error.isEmpty()) {
            const QString message = error.value("message").toString();
            if (!message.isEmpty())
                return message;
        }
        for (const char *key : {"message", "detail", "error"}) {
            const QString value = obj.value(QLatin1String(key)).toString();
            if (!value.isEmpty())
                return value;
        }
    }
    const QString text = QString::fromUtf8(body).trimmed();
    return text.left(300);
}

QString shellForStylePrompt(const QString &stylePrompt) {
    return stylePrompt.trimmed();
}

} // namespace

TTSEngine::TTSEngine(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audio);

    // 只有真正播完（或文件无法解码）才算结束。用 StoppedState 判定会把"用户主动停止"
    // 也当成播放完成。
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            emit playbackFinished();
        } else if (status == QMediaPlayer::InvalidMedia) {
            emit synthesisError(QString("音频播放失败: %1").arg(m_player->errorString()));
        }
    });
}

TTSEngine::~TTSEngine() {
    // 析构期间不要再回调上层：此时 MainWindow 可能已经拆掉一半了。
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    m_player->stop();
    delete m_tempFile;
}

QStringList TTSEngine::defaultModels() {
    return {"mimo-v2.5-tts", "mimo-v2.5-tts-voicedesign", "mimo-v2.5-tts-voiceclone"};
}

TTSEngine::ApiMode TTSEngine::apiModeFromString(const QString &value) {
    return value.compare(QLatin1String("speech"), Qt::CaseInsensitive) == 0
        ? ApiMode::OpenAiSpeech
        : ApiMode::ChatCompletions;
}

QString TTSEngine::apiModeToString(ApiMode mode) {
    return mode == ApiMode::OpenAiSpeech ? QStringLiteral("speech") : QStringLiteral("chat");
}

bool TTSEngine::isLocalEndpoint(const QString &apiBase) {
    const QString host = QUrl(apiBase.trimmed()).host().toLower();
    if (host.isEmpty())
        return false;

    if (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1") || host == QLatin1String("0.0.0.0"))
        return true;

    if (host.endsWith(QLatin1String(".local")) || host.endsWith(QLatin1String(".localhost")))
        return true;

    // IPv6 唯一本地地址 / 链路本地地址
    if (host.startsWith(QLatin1String("fc")) || host.startsWith(QLatin1String("fd"))
        || host.startsWith(QLatin1String("fe80:")))
        return true;

    // IPv4 私有网段
    const QStringList parts = host.split('.');
    if (parts.size() == 4) {
        bool ok = false;
        const int a = parts[0].toInt(&ok);
        if (!ok) return false;
        const int b = parts[1].toInt(&ok);
        if (!ok) return false;
        if (a == 10) return true;
        if (a == 192 && b == 168) return true;
        if (a == 172 && b >= 16 && b <= 31) return true;
        if (a == 169 && b == 254) return true;
    }
    return false;
}

QByteArray TTSEngine::wrapPcmAsWav(const QByteArray &pcm, int sampleRate, int channels,
                                   int bitsPerSample) {
    const int blockAlign = channels * bitsPerSample / 8;
    const int byteRate = sampleRate * blockAlign;

    QByteArray wav;
    wav.reserve(44 + pcm.size());
    QDataStream out(&wav, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData("RIFF", 4);
    out << quint32(36 + pcm.size());
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << quint32(16);              // fmt chunk 大小
    out << quint16(1);               // 编码格式: PCM
    out << quint16(channels);
    out << quint32(sampleRate);
    out << quint32(byteRate);
    out << quint16(blockAlign);
    out << quint16(bitsPerSample);
    out.writeRawData("data", 4);
    out << quint32(pcm.size());

    wav.append(pcm);
    return wav;
}

bool TTSEngine::beginRequest(QNetworkReply *reply) {
    if (m_activeReply) {
        // 上一请求还没结束，避免两个请求共用一个临时文件互相覆盖。
        emit synthesisError("已有请求正在进行，请先停止");
        reply->deleteLater();
        return false;
    }
    m_activeReply = reply;
    m_cancelled = false;
    return true;
}

void TTSEngine::fetchModels(const QString &apiBase, const QString &apiKey, ApiMode mode) {
    Q_UNUSED(mode)

    const QString base = normalizedBase(apiBase);
    if (base.isEmpty()) {
        emit modelsError("API 地址不能为空");
        return;
    }
    if (apiKey.isEmpty() && !isLocalEndpoint(base)) {
        emit modelsError("API Key 不能为空（本地/内网地址可留空）");
        return;
    }

    QNetworkRequest req{QUrl(base + "/models")};
    applyAuthHeaders(req, apiKey);
    req.setRawHeader("Accept", "application/json");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    req.setTransferTimeout(15000);
#endif

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_activeReply == reply) {
            m_activeReply = nullptr;
        }
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit modelsError(QString("获取模型列表失败: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QStringList models;

        // 兼容三种常见返回：裸数组、{data:[...]}、{models:[...]}（Ollama 风格）
        QJsonArray arr;
        if (doc.isArray())
            arr = doc.array();
        else if (doc.isObject())
            arr = doc.object().value("data").toArray();
        if (arr.isEmpty() && doc.isObject())
            arr = doc.object().value("models").toArray();

        for (const auto &item : arr) {
            const QJsonObject obj = item.toObject();
            QString id = obj.value("id").toString();
            if (id.isEmpty())
                id = obj.value("name").toString();   // Ollama 用 name
            if (!id.isEmpty() && !models.contains(id))
                models.append(id);
        }

        if (models.isEmpty()) {
            emit modelsError("服务端未返回任何模型，请手动填写模型名");
            return;
        }

        emit modelsReceived(models);
    });
}

void TTSEngine::synthesize(const Request &request) {
    const QString base = normalizedBase(request.apiBase);
    const QString text = request.text.trimmed();

    if (base.isEmpty()) {
        emit synthesisError("API 地址不能为空");
        return;
    }
    if (request.apiKey.isEmpty() && !isLocalEndpoint(base)) {
        emit synthesisError("API Key 不能为空（本地/内网地址可留空）");
        return;
    }
    if (text.isEmpty()) {
        emit synthesisError("合成文本不能为空");
        return;
    }

    if (request.mode == ApiMode::OpenAiSpeech) {
        QJsonObject payload;
        payload["model"] = request.model;
        payload["input"] = text;
        payload["voice"] = request.voice;
        payload["response_format"] = request.format.compare("pcm16", Qt::CaseInsensitive) == 0
            ? QStringLiteral("pcm")
            : QStringLiteral("wav");
        const QString style = shellForStylePrompt(request.stylePrompt);
        if (!style.isEmpty())
            payload["instructions"] = style;   // gpt-4o-mini-tts 及部分本地服务支持

        postRequest(request, payload, QStringLiteral("/audio/speech"), QStringLiteral("tts_"));
        return;
    }

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = shellForStylePrompt(request.stylePrompt);
    messages.append(userMsg);

    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = text;
    messages.append(assistantMsg);

    QJsonObject audioCfg;
    audioCfg["format"] = request.format;
    if (request.model != QLatin1String("mimo-v2.5-tts-voicedesign"))
        audioCfg["voice"] = request.voice;

    QJsonObject payload;
    payload["model"] = request.model;
    payload["messages"] = messages;
    payload["audio"] = audioCfg;

    postRequest(request, payload, QStringLiteral("/chat/completions"), QStringLiteral("tts_"));
}

void TTSEngine::cloneVoice(const Request &request, const QString &referenceAudioPath) {
    if (request.mode != ApiMode::ChatCompletions) {
        emit synthesisError("音色复刻需要 MiMo 的 /chat/completions 协议，"
                            "当前为 OpenAI /audio/speech 模式");
        return;
    }

    const QString base = normalizedBase(request.apiBase);
    const QString text = request.text.trimmed();

    if (base.isEmpty()) {
        emit synthesisError("API 地址不能为空");
        return;
    }
    if (request.apiKey.isEmpty() && !isLocalEndpoint(base)) {
        emit synthesisError("API Key 不能为空（本地/内网地址可留空）");
        return;
    }
    if (text.isEmpty()) {
        emit synthesisError("合成文本不能为空");
        return;
    }
    if (referenceAudioPath.isEmpty() || !QFile::exists(referenceAudioPath)) {
        emit synthesisError("请选择参考音频文件");
        return;
    }
    if (!referenceAudioPath.endsWith(".wav", Qt::CaseInsensitive)
        && !referenceAudioPath.endsWith(".mp3", Qt::CaseInsensitive)) {
        emit synthesisError("只支持 .wav 和 .mp3 格式的参考音频");
        return;
    }

    QFile audioFile(referenceAudioPath);
    if (!audioFile.open(QIODevice::ReadOnly)) {
        emit synthesisError("无法读取音频文件");
        return;
    }
    const QByteArray audioData = audioFile.readAll();
    audioFile.close();

    if (audioData.size() > 10 * 1024 * 1024) {
        emit synthesisError("音频文件不能超过 10MB");
        return;
    }

    const QString mimeType = referenceAudioPath.endsWith(".mp3", Qt::CaseInsensitive)
        ? QStringLiteral("audio/mpeg")
        : QStringLiteral("audio/wav");
    const QString base64Audio = "data:" + mimeType + ";base64," + QString::fromLatin1(audioData.toBase64());

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = shellForStylePrompt(request.stylePrompt);
    messages.append(userMsg);

    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = text;
    messages.append(assistantMsg);

    QJsonObject audioCfg;
    audioCfg["format"] = request.format;
    audioCfg["voice"] = base64Audio;

    QJsonObject payload;
    payload["model"] = QStringLiteral("mimo-v2.5-tts-voiceclone");
    payload["messages"] = messages;
    payload["audio"] = audioCfg;

    postRequest(request, payload, QStringLiteral("/chat/completions"), QStringLiteral("tts_clone_"));
}

void TTSEngine::postRequest(const Request &request, const QJsonObject &payload,
                            const QString &endpoint, const QString &tempPrefix) {
    QNetworkRequest req{QUrl(normalizedBase(request.apiBase) + endpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json, audio/*");
    applyAuthHeaders(req, request.apiKey);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    req.setTransferTimeout(120000);
#endif

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!beginRequest(reply))
        return;

    const bool speechMode = request.mode == ApiMode::OpenAiSpeech;
    const QString format = request.format;

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, speechMode, format, tempPrefix]() {
        if (m_activeReply == reply)
            m_activeReply = nullptr;

        const bool cancelled = m_cancelled;
        m_cancelled = false;

        reply->deleteLater();

        if (cancelled) {
            emit synthesisCancelled();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const QString detail = extractApiError(body);
            emit synthesisError(QString("API 请求失败: HTTP %1 - %2%3")
                                    .arg(httpStatus)
                                    .arg(reply->errorString())
                                    .arg(detail.isEmpty() ? QString() : QStringLiteral("\n") + detail));
            return;
        }

        if (speechMode)
            handleSpeechReply(reply, format, tempPrefix);
        else
            handleChatReply(reply, format, tempPrefix);
    });
}

void TTSEngine::handleChatReply(QNetworkReply *reply, const QString &format,
                                const QString &tempPrefix) {
    const QByteArray responseBody = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty()) {
        emit synthesisError("返回数据格式错误: " + QString::fromUtf8(responseBody).left(200));
        return;
    }

    const QString audioData =
        choices.at(0).toObject().value("message").toObject()
            .value("audio").toObject().value("data").toString();
    if (audioData.isEmpty()) {
        emit synthesisError("返回数据中没有音频内容");
        return;
    }

    writeAudioToTempFile(QByteArray::fromBase64(audioData.toUtf8()), format, tempPrefix);
}

void TTSEngine::handleSpeechReply(QNetworkReply *reply, const QString &format,
                                  const QString &tempPrefix) {
    const QByteArray body = reply->readAll();
    if (body.isEmpty()) {
        emit synthesisError("服务端返回了空音频");
        return;
    }

    // 有些服务即使 HTTP 200 也会在 body 里塞 JSON 错误。
    if (body.startsWith('{')) {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            if (obj.contains("error") || obj.contains("detail")) {
                emit synthesisError("服务端返回错误: " + extractApiError(body));
                return;
            }
        }
    }

    writeAudioToTempFile(body, format, tempPrefix);
}

void TTSEngine::writeAudioToTempFile(const QByteArray &audioBytes, const QString &format,
                                     const QString &tempPrefix) {
    if (audioBytes.isEmpty()) {
        emit synthesisError("音频数据为空");
        return;
    }

    // 裸 PCM 没有容器头，播放器认不出来，本地统一封装成 WAV。
    const bool isPcm = format.compare("pcm16", Qt::CaseInsensitive) == 0
        || format.compare("pcm", Qt::CaseInsensitive) == 0;
    const QByteArray payload = isPcm ? wrapPcmAsWav(audioBytes) : audioBytes;

    m_player->stop();
    delete m_tempFile;
    m_tempFile = new QTemporaryFile(QDir::tempPath() + "/" + tempPrefix + "XXXXXX.wav");
    if (!m_tempFile->open()) {
        emit synthesisError("无法创建临时文件");
        return;
    }
    m_tempFile->write(payload);
    m_tempFile->flush();

    emit synthesisFinished(m_tempFile->fileName());
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
    if (m_activeReply) {
        m_cancelled = true;
        m_activeReply->abort();
    }
    m_player->stop();
}

bool TTSEngine::isPlaying() const {
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}
