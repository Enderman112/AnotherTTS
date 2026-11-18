#pragma once

#include <QString>
#include <QStringList>
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

    // "chat" = MiMo /chat/completions，"speech" = OpenAI 标准 /audio/speech，
    // "deepseek" = DeepSeek 工具 API /tools/tts
    QString apiMode() const;
    void setApiMode(const QString &value);

    QString model() const;
    void setModel(const QString &value);

    QString voice() const;
    void setVoice(const QString &value);

    QString outputFormat() const;
    void setOutputFormat(const QString &value);

    QStringList voices() const;
    void setVoices(const QStringList &value);

    // 模型列表只显示含 "tts" 的条目。本地服务的模型名往往不含 tts，可关掉。
    bool onlyTtsModels() const;
    void setOnlyTtsModels(bool value);

private:
    QString configPath() const;

    QString m_apiBase = "https://api.xiaomimimo.com/v1";
    QString m_apiKey;
    QString m_apiMode = "chat";
    QString m_model = "mimo-v2.5-tts";
    QString m_voice = "mimo_default";
    QString m_outputFormat = "wav";
    QStringList m_voices = {"mimo_default", "冰糖", "茉莉", "苏打", "白桦", "Mia", "Chloe", "Milo", "Dean"};
    bool m_onlyTtsModels = true;
};
