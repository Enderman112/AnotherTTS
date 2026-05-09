#include "config.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

Config::Config() {
    load();
}

QString Config::configPath() const {
    return QDir::homePath() + "/.tts_gui_config.json";
}

void Config::load() {
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    if (obj.contains("api_base")) m_apiBase = obj["api_base"].toString();
    if (obj.contains("api_key")) m_apiKey = obj["api_key"].toString();
    if (obj.contains("model")) m_model = obj["model"].toString();
    if (obj.contains("voice")) m_voice = obj["voice"].toString();
    if (obj.contains("output_format")) m_outputFormat = obj["output_format"].toString();
    if (obj.contains("voices")) {
        m_voices.clear();
        QJsonArray arr = obj["voices"].toArray();
        for (const auto &v : arr) {
            m_voices.append(v.toString());
        }
    }
}

void Config::save() {
    QJsonObject obj;
    obj["api_base"] = m_apiBase;
    obj["api_key"] = m_apiKey;
    obj["model"] = m_model;
    obj["voice"] = m_voice;
    obj["output_format"] = m_outputFormat;

    QJsonArray voicesArr;
    for (const auto &v : m_voices) {
        voicesArr.append(v);
    }
    obj["voices"] = voicesArr;

    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}

QString Config::apiBase() const { return m_apiBase; }
void Config::setApiBase(const QString &v) { m_apiBase = v; }

QString Config::apiKey() const { return m_apiKey; }
void Config::setApiKey(const QString &v) { m_apiKey = v; }

QString Config::model() const { return m_model; }
void Config::setModel(const QString &v) { m_model = v; }

QString Config::voice() const { return m_voice; }
void Config::setVoice(const QString &v) { m_voice = v; }

QString Config::outputFormat() const { return m_outputFormat; }
void Config::setOutputFormat(const QString &v) { m_outputFormat = v; }

QStringList Config::voices() const { return m_voices; }
void Config::setVoices(const QStringList &v) { m_voices = v; }
