#include "mainwindow.h"
#include "config.h"
#include "ttsengine.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_config(new Config)
    , m_engine(new TTSEngine(this))
{
    setWindowTitle("MiMo TTS 文本朗读工具");
    resize(700, 650);
    setMinimumSize(600, 550);

    buildUi();
    loadConfig();

    connect(m_engine, &TTSEngine::synthesisFinished, this, [this](const QString &path) {
        m_statusLabel->setText("正在播放...");
        m_engine->play(path);
    });

    connect(m_engine, &TTSEngine::synthesisError, this, [this](const QString &err) {
        QMessageBox::critical(this, "错误", err);
        m_statusLabel->setText("出错");
        m_playBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
    });

    connect(m_engine, &TTSEngine::playbackFinished, this, [this]() {
        m_statusLabel->setText("播放完成");
        m_playBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
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

    // API 配置
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

    // 语音参数
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
    m_voiceCombo->addItems(TTSEngine::voices());
    paramLayout->addWidget(m_voiceCombo, 0, 4);

    paramLayout->addWidget(new QLabel("格式:"), 1, 0);
    m_formatCombo = new QComboBox;
    m_formatCombo->addItems(TTSEngine::formats());
    paramLayout->addWidget(m_formatCombo, 1, 1);

    mainLayout->addWidget(paramGroup);

    // 风格控制
    auto *styleGroup = new QGroupBox("风格控制 (可选，自然语言描述语气风格)");
    auto *styleLayout = new QVBoxLayout(styleGroup);
    m_styleEdit = new QPlainTextEdit;
    m_styleEdit->setMaximumHeight(80);
    styleLayout->addWidget(m_styleEdit);
    mainLayout->addWidget(styleGroup);

    // 合成文本
    auto *textGroup = new QGroupBox("合成文本");
    auto *textLayout = new QVBoxLayout(textGroup);
    m_textEdit = new QPlainTextEdit;
    m_textEdit->setMinimumHeight(100);
    textLayout->addWidget(m_textEdit);
    mainLayout->addWidget(textGroup, 1);

    // 按钮栏
    auto *btnLayout = new QHBoxLayout;
    m_playBtn = new QPushButton("播放");
    m_stopBtn = new QPushButton("停止");
    m_stopBtn->setEnabled(false);
    m_saveBtn = new QPushButton("保存音频");
    auto *saveConfigBtn = new QPushButton("保存配置");

    btnLayout->addWidget(m_playBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(saveConfigBtn);

    mainLayout->addLayout(btnLayout);

    // 状态栏
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    mainLayout->addWidget(m_statusLabel);

    // 连接信号
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSave);
    connect(saveConfigBtn, &QPushButton::clicked, this, &MainWindow::onSaveConfig);
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

void MainWindow::onPlay() {
    QString text = m_textEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要朗读的文本");
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API Key");
        return;
    }

    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_statusLabel->setText("正在合成语音...");

    m_engine->synthesize(text,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         m_modelCombo->currentText(),
                         m_voiceCombo->currentText(),
                         m_formatCombo->currentText(),
                         m_styleEdit->toPlainText().trimmed());
}

void MainWindow::onStop() {
    m_engine->stop();
    m_statusLabel->setText("已停止");
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void MainWindow::onSave() {
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
    m_playBtn->setEnabled(false);

    connect(m_engine, &TTSEngine::synthesisFinished, this, [this, filePath](const QString &srcPath) {
        QFile::remove(filePath);
        if (QFile::copy(srcPath, filePath)) {
            QMessageBox::information(this, "成功", "音频已保存到:\n" + filePath);
            m_statusLabel->setText("保存完成");
        } else {
            QMessageBox::critical(this, "错误", "保存失败");
            m_statusLabel->setText("保存失败");
        }
        m_playBtn->setEnabled(true);
    }, Qt::SingleShotConnection);

    m_engine->synthesize(text,
                         m_apiBaseEdit->text().trimmed(),
                         m_apiKeyEdit->text().trimmed(),
                         m_modelCombo->currentText(),
                         m_voiceCombo->currentText(),
                         ext,
                         m_styleEdit->toPlainText().trimmed());
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
