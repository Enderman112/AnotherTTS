#pragma once

#include <QMainWindow>

class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class Config;
class TTSEngine;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPlay();
    void onStop();
    void onSave();
    void onSaveConfig();
    void onFetchModels();
    void onVoiceSettings();

private:
    void buildUi();
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
    QPushButton *m_playBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_saveBtn;
    QLabel *m_statusLabel;
};
