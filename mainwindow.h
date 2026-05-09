#pragma once

#include <QMainWindow>

class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QTabWidget;
class Config;
class TTSEngine;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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

private:
    void buildUi();
    QWidget *buildTtsTab();
    QWidget *buildCloneTab();
    void loadConfig();

    Config *m_config;
    TTSEngine *m_engine;

    QLineEdit *m_apiBaseEdit;
    QLineEdit *m_apiKeyEdit;

    QComboBox *m_modelCombo;
    QComboBox *m_voiceCombo;
    QComboBox *m_formatCombo;
    QPlainTextEdit *m_styleEdit;
    QPlainTextEdit *m_textEdit;
    QPushButton *m_ttsPlayBtn;
    QPushButton *m_ttsStopBtn;
    QPushButton *m_ttsSaveBtn;

    QLineEdit *m_audioFileEdit;
    QPlainTextEdit *m_cloneTextEdit;
    QPushButton *m_clonePlayBtn;
    QPushButton *m_cloneStopBtn;
    QPushButton *m_cloneSaveBtn;

    QLabel *m_statusLabel;
};
