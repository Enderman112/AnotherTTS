#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryFile>
#include <QPointer>
#include <QJsonObject>
#include <QStringList>

class TTSEngine : public QObject {
    Q_OBJECT

public:
    // 接口协议。
    // ChatCompletions: MiMo 私有协议，POST /chat/completions，音频以 base64 内嵌在 JSON 里。
    // OpenAiSpeech:    OpenAI 标准协议，POST /audio/speech，响应体是原始音频字节。
    //                  vLLM / LM Studio / LocalAI / Speaches / Kokoro-FastAPI 等本地服务都用这个。
    // DeepSeekTts:     DeepSeek 工具 API，POST /tools/tts，响应体是原始音频字节。
    //                  没有 model，只有 voice_id(stella/tide)，不支持风格与音色复刻。
    enum class ApiMode {
        ChatCompletions,
        OpenAiSpeech,
        DeepSeekTts,
    };

    struct Request {
        QString text;
        QString apiBase;
        QString apiKey;
        QString model;
        QString voice;
        QString format;       // "wav" 或 "pcm16"
        QString stylePrompt;
        ApiMode mode = ApiMode::ChatCompletions;
    };

    explicit TTSEngine(QObject *parent = nullptr);
    ~TTSEngine() override;

    void fetchModels(const QString &apiBase, const QString &apiKey, ApiMode mode = ApiMode::ChatCompletions);
    void synthesize(const Request &request);
    void cloneVoice(const Request &request, const QString &referenceAudioPath);
    void play(const QString &filePath);
    void stop();
    bool isPlaying() const;

    static QStringList defaultModels();
    static ApiMode apiModeFromString(const QString &value);
    static QString apiModeToString(ApiMode mode);
    // 判断是否指向本机/内网地址。本地服务通常不需要 API Key。
    static bool isLocalEndpoint(const QString &apiBase);
    // 本地服务把裸 PCM 存成 WAV，否则播放器无法识别。
    static QByteArray wrapPcmAsWav(const QByteArray &pcm,
                                   int sampleRate = 24000,
                                   int channels = 1,
                                   int bitsPerSample = 16);

signals:
    void modelsReceived(const QStringList &models);
    void modelsError(const QString &error);
    void synthesisFinished(const QString &filePath);
    void synthesisError(const QString &error);
    void synthesisCancelled();
    void playbackFinished();

private:
    void postRequest(const Request &request, const QJsonObject &payload, const QString &endpoint,
                     const QString &tempPrefix);
    void handleChatReply(QNetworkReply *reply, const QString &format, const QString &tempPrefix);
    void handleSpeechReply(QNetworkReply *reply, const QString &format, const QString &tempPrefix);
    void writeAudioToTempFile(const QByteArray &audioBytes, const QString &format,
                              const QString &tempPrefix);
    bool beginRequest(QNetworkReply *reply);

    QNetworkAccessManager *m_nam;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QTemporaryFile *m_tempFile = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    bool m_cancelled = false;
};
