#pragma once

#include <QDialog>

class QListWidget;
class QLineEdit;
class Config;

class VoiceSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit VoiceSettingsDialog(Config *config, QWidget *parent = nullptr);

    QStringList voices() const;

private slots:
    void onAdd();
    void onRemove();

private:
    void loadVoices();

    Config *m_config;
    QListWidget *m_listWidget;
    QLineEdit *m_nameEdit;
};
