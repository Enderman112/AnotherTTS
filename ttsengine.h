#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryFile>

class TTSEngine : public QObject {
    Q_OBJECT

public:
    explicit TTSEngine(QObject *parent = nullptr);
    ~TTSEngine();

    void fetchModels(const QString &apiBase, const QString &apiKey);
    void synthesize(const QString &text, const QString &apiBase, const QString &apiKey,
                    const QString &model, const QString &voice, const QString &format,
                    const QString &stylePrompt = QString());
    void cloneVoice(const QString &text, const QString &audioFilePath,
                    const QString &apiBase, const QString &apiKey, const QString &format);
    void play(const QString &filePath);
    void stop();
    bool isPlaying() const;

    static QStringList voices();
    static QStringList defaultModels();
    static QStringList formats();

signals:
    void modelsReceived(const QStringList &models);
    void modelsError(const QString &error);
    void synthesisFinished(const QString &filePath);
    void synthesisError(const QString &error);
    void playbackFinished();

private:
    QNetworkAccessManager *m_nam;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QTemporaryFile *m_tempFile = nullptr;
};
