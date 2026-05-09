#include "mainwindow.h"
#include "config.h"
#include "ttsengine.h"
#include "voicesettingsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_config(new Config)
    , m_engine(new TTSEngine(this))
{
    setWindowTitle("MiMo TTS 文本朗读工具");
    resize(700, 700);
    setMinimumSize(600, 600);

    buildUi();
    loadConfig();

    connect(m_engine, &TTSEngine::synthesisFinished, this, [this](const QString &path) {
        m_statusLabel->setText("正在播放...");
        m_engine->play(path);
    });

    connect(m_engine, &TTSEngine::synthesisError, this, [this](const QString &err) {
        QMessageBox::critical(this, "错误", err);
        m_statusLabel->setText("出错");
        m_ttsPlayBtn->setEnabled(true);
        m_ttsStopBtn->setEnabled(false);
        m_clonePlayBtn->setEnabled(true);
        m_cloneStopBtn->setEnabled(false);
    });

    connect(m_engine, &TTSEngine::playbackFinished, this, [this]() {
        m_statusLabel->setText("播放完成");
        m_ttsPlayBtn->setEnabled(true);
        m_ttsStopBtn->setEnabled(false);
        m_clonePlayBtn->setEnabled(true);
        m_cloneStopBtn->setEnabled(false);
    });

    connect(m_engine, &TTSEngine::modelsReceived, this, [this](const QStringList &models) {
        QString current = m_modelCombo->currentText();
        m_modelCombo->clear();
        m_modelCombo->addItems(models);
        int idx = m_modelCombo->findText(current);
        if (idx >= 0) m_modelCombo->setCurrentIndex(idx);
        m_statusLabel->setText("模型列表已更新");
    });

    connect(m_engine, &TTSEngine::modelsError, this, [this](const QString &err) {
        m_statusLabel->setText("获取模型失败: " + err);
    });
}

MainWindow::~MainWindow() {}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    auto *apiGroup = new QGroupBox("API 配置");
    auto *apiLayout = new QFormLayout(apiGroup);

    m_apiBaseEdit = new QLineEdit;
    m_apiBaseEdit->setMinimumWidth(500);
    apiLayout->addRow("API 地址:", m_apiBaseEdit);

    m_apiKeyEdit = new QLineEdit;
    m_apiKeyEdit->setMinimumWidth(500);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    auto *keyLayout = new QHBoxLayout;
    keyLayout->addWidget(m_apiKeyEdit);
    auto *showKeyCheck = new QCheckBox("显示");
    connect(showKeyCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    keyLayout->addWidget(showKeyCheck);
    apiLayout->addRow("API Key:", keyLayout);

    mainLayout->addWidget(apiGroup);

    auto *tabWidget = new QTabWidget;
    tabWidget->addTab(buildTtsTab(), "TTS 合成");
    tabWidget->addTab(buildCloneTab(), "音色复刻");
    mainLayout->addWidget(tabWidget, 1);

    auto *btnLayout = new QHBoxLayout;
    auto *saveConfigBtn = new QPushButton("保存配置");
    connect(saveConfigBtn, &QPushButton::clicked, this, &MainWindow::onSaveConfig);
    btnLayout->addStretch();
    btnLayout->addWidget(saveConfigBtn);
    mainLayout->addLayout(btnLayout);

    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    mainLayout->addWidget(m_statusLabel);
}

QWidget *MainWindow::buildTtsTab() {
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *paramGroup = new QGroupBox("语音参数");
    auto *paramLayout = new QGridLayout(paramGroup);

    paramLayout->addWidget(new QLabel("模型:"), 0, 0);
    m_modelCombo = new QComboBox;
    m_modelCombo->addItems(TTSEngine::defaultModels());
    m_modelCombo->setMinimumWidth(200);
    paramLayout->addWidget(m_modelCombo, 0, 1);

    auto *refreshModelsBtn = new QPushButton("刷新");
    connect(refreshModelsBtn, &QPushButton::clicked, this, &MainWindow::onFetchModels);
    paramLayout->addWidget(refreshModelsBtn, 0, 2);

    paramLayout->addWidget(new QLabel("声音:"), 0, 3);
    m_voiceCombo = new QComboBox;
    m_voiceCombo->addItems(m_config->voices());
    paramLayout->addWidget(m_voiceCombo, 0, 4);

    auto *voiceSettingsBtn = new QPushButton("设置");
    connect(voiceSettingsBtn, &QPushButton::clicked, this, &MainWindow::onVoiceSettings);
    paramLayout->addWidget(voiceSettingsBtn, 0, 5);

    paramLayout->addWidget(new QLabel("格式:"), 1, 0);
    m_formatCombo = new QComboBox;
    m_formatCombo->addItems(TTSEngine::formats());
    paramLayout->addWidget(m_formatCombo, 1, 1);

    layout->addWidget(paramGroup);

    auto *styleGroup = new QGroupBox("风格控制 (可选，自然语言描述语气风格)");
    auto *styleLayout = new QVBoxLayout(styleGroup);
    m_styleEdit = new QPlainTextEdit;
    m_styleEdit->setMaximumHeight(80);
    styleLayout->addWidget(m_styleEdit);
    layout->addWidget(styleGroup);

    auto *textGroup = new QGroupBox("合成文本");
    auto *textLayout = new QVBoxLayout(textGroup);
    m_textEdit = new QPlainTextEdit;
    m_textEdit->setMinimumHeight(100);
    textLayout->addWidget(m_textEdit);
    layout->addWidget(textGroup, 1);

    auto *btnLayout = new QHBoxLayout;
    m_ttsPlayBtn = new QPushButton("播放");
    m_ttsStopBtn = new QPushButton("停止");
    m_ttsStopBtn->setEnabled(false);
    m_ttsSaveBtn = new QPushButton("保存音频");

    btnLayout->addWidget(m_ttsPlayBtn);
    btnLayout->addWidget(m_ttsStopBtn);
    btnLayout->addWidget(m_ttsSaveBtn);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);

    connect(m_ttsPlayBtn, &QPushButton::clicked, this, &MainWindow::onTtsPlay);
    connect(m_ttsStopBtn, &QPushButton::clicked, this, &MainWindow::onTtsStop);
    connect(m_ttsSaveBtn, &QPushButton::clicked, this, &MainWindow::onTtsSave);

    return tab;
}

QWidget *MainWindow::buildCloneTab() {
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *audioGroup = new QGroupBox("参考音频");
    auto *audioLayout = new QHBoxLayout(audioGroup);

    m_audioFileEdit = new QLineEdit;
    m_audioFileEdit->setPlaceholderText("选择参考音频文件...");
    audioLayout->addWidget(m_audioFileEdit);

    auto *selectBtn = new QPushButton("选择文件");
    connect(selectBtn, &QPushButton::clicked, this, &MainWindow::onSelectAudioFile);
    audioLayout->addWidget(selectBtn);

    layout->addWidget(audioGroup);

    auto *textGroup = new QGroupBox("合成文本");
    auto *textLayout = new QVBoxLayout(textGroup);
    m_cloneTextEdit = new QPlainTextEdit;
    m_cloneTextEdit->setMinimumHeight(150);
    textLayout->addWidget(m_cloneTextEdit);
    layout->addWidget(textGroup, 1);

    auto *hintLabel = new QLabel("提示: 使用 mimo-v2.5-tts-voiceclone 模型，上传参考音频进行音色复刻");
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto *btnLayout = new QHBoxLayout;
    m_clonePlayBtn = new QPushButton("播放");
    m_cloneStopBtn = new QPushButton("停止");
    m_cloneStopBtn->setEnabled(false);
    m_cloneSaveBtn = new QPushButton("保存音频");

    btnLayout->addWidget(m_clonePlayBtn);
    btnLayout->addWidget(m_cloneStopBtn);
    btnLayout->addWidget(m_cloneSaveBtn);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);

    connect(m_clonePlayBtn, &QPushButton::clicked, this, &MainWindow::onClonePlay);
    connect(m_cloneStopBtn, &QPushButton::clicked, this, &MainWindow::onCloneStop);
    connect(m_cloneSaveBtn, &QPushButton::clicked, this, &MainWindow::onCloneSave);

    return tab;
}

void MainWindow::loadConfig() {
    m_apiBaseEdit->setText(m_config->apiBase());
    m_apiKeyEdit->setText(m_config->apiKey());

    int modelIdx = m_modelCombo->findText(m_config->model());
    if (modelIdx >= 0) m_modelCombo->setCurrentIndex(modelIdx);

    int voiceIdx = m_voiceCombo->findText(m_config->voice());
    if (voiceIdx >= 0) m_voiceCombo->setCurrentIndex(voiceIdx);

    int formatIdx = m_formatCombo->findText(m_config->outputFormat());
    if (formatIdx >= 0) m_formatCombo->setCurrentIndex(formatIdx);
}

void MainWindow::onTtsPlay() {
    QString text = m_textEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要朗读的文本");
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Key");
        return;
    }

    m_ttsPlayBtn->setEnabled(false);
    m_ttsStopBtn->setEnabled(true);
    m_statusLabel->setText("正在合成语音...");

    m_engine->synthesize(text,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         m_modelCombo->currentText(),
                         m_voiceCombo->currentText(),
                         m_formatCombo->currentText(),
                         m_styleEdit->toPlainText().trimmed());
}

void MainWindow::onTtsStop() {
    m_engine->stop();
    m_statusLabel->setText("已停止");
    m_ttsPlayBtn->setEnabled(true);
    m_ttsStopBtn->setEnabled(false);
}

void MainWindow::onTtsSave() {
    QString text = m_textEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要朗读的文本");
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Key");
        return;
    }

    QString ext = m_formatCombo->currentText();
    QString filePath = QFileDialog::getSaveFileName(this, "保存音频", "",
        QString("%1 文件 (*.%2);;所有文件 (*.*)").arg(ext.toUpper(), ext));
    if (filePath.isEmpty())
        return;

    m_statusLabel->setText("正在合成并保存...");
    m_ttsPlayBtn->setEnabled(false);

    connect(m_engine, &TTSEngine::synthesisFinished, this, [this, filePath](const QString &srcPath) {
        QFile::remove(filePath);
        if (QFile::copy(srcPath, filePath)) {
            QMessageBox::information(this, "成功", "音频已保存到:\n" + filePath);
            m_statusLabel->setText("保存完成");
        } else {
            QMessageBox::critical(this, "错误", "保存失败");
            m_statusLabel->setText("保存失败");
        }
        m_ttsPlayBtn->setEnabled(true);
    }, Qt::SingleShotConnection);

    m_engine->synthesize(text,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         m_modelCombo->currentText(),
                         m_voiceCombo->currentText(),
                         ext,
                         m_styleEdit->toPlainText().trimmed());
}

void MainWindow::onClonePlay() {
    QString text = m_cloneTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要合成的文本");
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Key");
        return;
    }
    QString audioFile = m_audioFileEdit->text().trimmed();
    if (audioFile.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择参考音频文件");
        return;
    }

    m_clonePlayBtn->setEnabled(false);
    m_cloneStopBtn->setEnabled(true);
    m_statusLabel->setText("正在合成语音...");

    m_engine->cloneVoice(text, audioFile,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         m_formatCombo->currentText());
}

void MainWindow::onCloneStop() {
    m_engine->stop();
    m_statusLabel->setText("已停止");
    m_clonePlayBtn->setEnabled(true);
    m_cloneStopBtn->setEnabled(false);
}

void MainWindow::onCloneSave() {
    QString text = m_cloneTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要合成的文本");
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Key");
        return;
    }
    QString audioFile = m_audioFileEdit->text().trimmed();
    if (audioFile.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择参考音频文件");
        return;
    }

    QString ext = m_formatCombo->currentText();
    QString filePath = QFileDialog::getSaveFileName(this, "保存音频", "",
        QString("%1 文件 (*.%2);;所有文件 (*.*)").arg(ext.toUpper(), ext));
    if (filePath.isEmpty())
        return;

    m_statusLabel->setText("正在合成并保存...");
    m_clonePlayBtn->setEnabled(false);

    connect(m_engine, &TTSEngine::synthesisFinished, this, [this, filePath](const QString &srcPath) {
        QFile::remove(filePath);
        if (QFile::copy(srcPath, filePath)) {
            QMessageBox::information(this, "成功", "音频已保存到:\n" + filePath);
            m_statusLabel->setText("保存完成");
        } else {
            QMessageBox::critical(this, "错误", "保存失败");
            m_statusLabel->setText("保存失败");
        }
        m_clonePlayBtn->setEnabled(true);
    }, Qt::SingleShotConnection);

    m_engine->cloneVoice(text, audioFile,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         ext);
}

void MainWindow::onSaveConfig() {
    m_config->setApiBase(m_apiBaseEdit->text().trimmed());
    m_config->setApiKey(m_apiKeyEdit->text().trimmed());
    m_config->setModel(m_modelCombo->currentText());
    m_config->setVoice(m_voiceCombo->currentText());
    m_config->setOutputFormat(m_formatCombo->currentText());
    m_config->save();
    QMessageBox::information(this, "成功", "配置已保存");
}

void MainWindow::onFetchModels() {
    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先输入 API Key");
        return;
    }
    m_statusLabel->setText("正在获取模型列表...");
    m_engine->fetchModels(m_apiBaseEdit->text().trimmed(), apiKey);
}

void MainWindow::onVoiceSettings() {
    VoiceSettingsDialog dlg(m_config, this);
    dlg.exec();

    QString current = m_voiceCombo->currentText();
    m_voiceCombo->clear();
    m_voiceCombo->addItems(m_config->voices());
    int idx = m_voiceCombo->findText(current);
    if (idx >= 0) m_voiceCombo->setCurrentIndex(idx);
}

void MainWindow::onSelectAudioFile() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择参考音频文件", "",
        "音频文件 (*.wav *.mp3 *.flac *.m4a *.ogg);;所有文件 (*.*)");
    if (!filePath.isEmpty()) {
        m_audioFileEdit->setText(filePath);
    }
}
