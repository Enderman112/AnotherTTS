#pragma once

#include <QString>
#include <QJsonObject>

class Config {
public:
    Config();

    void load();
    void save();

    QString apiBase() const;
    void setApiBase(const QString &value);

    QString apiKey() const;
    void setApiKey(const QString &value);

    QString model() const;
    void setModel(const QString &value);

    QString voice() const;
    void setVoice(const QString &value);

    QString outputFormat() const;
    void setOutputFormat(const QString &value);

private:
    QString configPath() const;

    QString m_apiBase = "https://api.xiaomimimo.com/v1";
    QString m_apiKey;
    QString m_model = "mimo-v2.5-tts";
    QString m_voice = "mimo_default";
    QString m_outputFormat = "wav";
};
