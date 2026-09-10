#pragma once

#include <QMainWindow>
#include <QStringList>
#include <memory>

#include "ttsengine.h"

class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QTabWidget;
class Config;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onTtsPlay();
    void onTtsStop();
    void onTtsSave();
    void onClonePlay();
    void onCloneStop();
    void onCloneSave();
    void onSaveConfig();
    void onFetchModels();
    void onVoiceSettings();
    void onSelectAudioFile();
    void onPresetChanged(int index);
    void onModelsReceived(const QStringList &models);
    void onSynthesisFinished(const QString &filePath);
    void onSynthesisError(const QString &error);
    void onSynthesisCancelled();
    void onPlaybackFinished();

private:
    enum class Tab { Tts, Clone };
    // 引擎只有一路请求，用它区分"合成后播放"和"合成后保存"。
    enum class PendingAction { None, Play, Save };

    void buildUi();
    QWidget *buildApiGroup();
    QWidget *buildTtsTab();
    QWidget *buildCloneTab();

    void loadConfig();
    void setBusy(Tab tab, bool busy);
    void applyPreset(int index);
    void syncPresetSelection();
    void applyModelFilter();
    void updateModeHint();
    bool checkReady(const QString &text);
    bool checkCloneReady(const QString &text);

    QString currentFormat() const;
    TTSEngine::Request currentRequest(const QString &text, const QString &format) const;

    std::unique_ptr<Config> m_config;
    TTSEngine *m_engine;

    // API 配置
    QComboBox *m_presetCombo;
    QComboBox *m_modeCombo;
    QLineEdit *m_apiBaseEdit;
    QLineEdit *m_apiKeyEdit;
    QLabel *m_modeHint;

    // TTS 标签页
    QComboBox *m_modelCombo;
    QComboBox *m_voiceCombo;
    QComboBox *m_formatCombo;
    QCheckBox *m_onlyTtsCheck;
    QPlainTextEdit *m_styleEdit;
    QPlainTextEdit *m_textEdit;
    QPushButton *m_ttsPlayBtn;
    QPushButton *m_ttsStopBtn;
    QPushButton *m_ttsSaveBtn;

    // 音色复刻标签页
    QLineEdit *m_audioFileEdit;
    QPlainTextEdit *m_cloneTextEdit;
    QPushButton *m_clonePlayBtn;
    QPushButton *m_cloneStopBtn;
    QPushButton *m_cloneSaveBtn;

    QLabel *m_statusLabel;

    QStringList m_allModels;
    PendingAction m_pendingAction = PendingAction::None;
    Tab m_pendingTab = Tab::Tts;
    QString m_pendingSavePath;
};
