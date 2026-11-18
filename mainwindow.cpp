#include "mainwindow.h"
#include "config.h"
#include "ttsengine.h"
#include "voicesettingsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
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
#include <QDir>
#include <QDateTime>
#include <QSignalBlocker>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

namespace {

struct ApiPreset {
    QString name;
    QString base;
    TTSEngine::ApiMode mode;
    QString model;
    QString voice;
};

// 本地服务的默认端口各不相同，这里给出常见几种，选中后自动填好地址/协议/模型/音色。
const QList<ApiPreset> &apiPresets() {
    static const QList<ApiPreset> presets = {
        {QStringLiteral("小米 MiMo（云端）"),
         QStringLiteral("https://api.xiaomimimo.com/v1"), TTSEngine::ApiMode::ChatCompletions,
         QStringLiteral("mimo-v2.5-tts"), QStringLiteral("mimo_default")},
        {QStringLiteral("DeepSeek 官方 TTS（云端）"),
         QStringLiteral("https://api.deepseek.com/v1"), TTSEngine::ApiMode::DeepSeekTts,
         QString(), QStringLiteral("stella")},
        {QStringLiteral("本地 Speaches / openedai-speech :8000"),
         QStringLiteral("http://127.0.0.1:8000/v1"), TTSEngine::ApiMode::OpenAiSpeech,
         QStringLiteral("tts-1"), QStringLiteral("alloy")},
        {QStringLiteral("本地 Kokoro-FastAPI :8880"),
         QStringLiteral("http://127.0.0.1:8880/v1"), TTSEngine::ApiMode::OpenAiSpeech,
         QStringLiteral("kokoro"), QStringLiteral("af_heart")},
        {QStringLiteral("本地 LocalAI :8080"),
         QStringLiteral("http://127.0.0.1:8080/v1"), TTSEngine::ApiMode::OpenAiSpeech,
         QStringLiteral("tts-1"), QStringLiteral("alloy")},
        {QStringLiteral("本地 LM Studio :1234"),
         QStringLiteral("http://127.0.0.1:1234/v1"), TTSEngine::ApiMode::OpenAiSpeech,
         QStringLiteral("tts-1"), QStringLiteral("alloy")},
        {QStringLiteral("自定义"),
         QString(), TTSEngine::ApiMode::ChatCompletions, QString(), QString()},
    };
    return presets;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_config(std::make_unique<Config>())
    , m_engine(new TTSEngine(this))
{
    setWindowTitle("TTS 文本朗读工具");
    resize(760, 760);
    setMinimumSize(640, 620);

    buildUi();
    loadConfig();

    connect(m_engine, &TTSEngine::modelsReceived, this, &MainWindow::onModelsReceived);
    connect(m_engine, &TTSEngine::modelsError, this, [this](const QString &err) {
        m_statusLabel->setText("获取模型失败: " + err);
    });
    connect(m_engine, &TTSEngine::synthesisFinished, this, &MainWindow::onSynthesisFinished);
    connect(m_engine, &TTSEngine::synthesisError, this, &MainWindow::onSynthesisError);
    connect(m_engine, &TTSEngine::synthesisCancelled, this, &MainWindow::onSynthesisCancelled);
    connect(m_engine, &TTSEngine::playbackFinished, this, &MainWindow::onPlaybackFinished);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    mainLayout->addWidget(buildApiGroup());

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

    auto *versionLabel = new QLabel(QStringLiteral("v") + QStringLiteral(APP_VERSION));
    versionLabel->setAlignment(Qt::AlignRight);
    auto palette = versionLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::gray);
    versionLabel->setPalette(palette);
    mainLayout->addWidget(versionLabel);
}

QWidget *MainWindow::buildApiGroup() {
    auto *apiGroup = new QGroupBox("API 配置");
    auto *apiLayout = new QFormLayout(apiGroup);

    // 预设：一键切到本地部署的服务
    m_presetCombo = new QComboBox;
    for (const auto &preset : apiPresets())
        m_presetCombo->addItem(preset.name);
    connect(m_presetCombo, &QComboBox::activated, this, &MainWindow::onPresetChanged);
    apiLayout->addRow("预设:", m_presetCombo);

    // 接口协议
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("MiMo /chat/completions（base64 音频）",
                         TTSEngine::apiModeToString(TTSEngine::ApiMode::ChatCompletions));
    m_modeCombo->addItem("OpenAI /audio/speech（原始音频，本地服务常用）",
                         TTSEngine::apiModeToString(TTSEngine::ApiMode::OpenAiSpeech));
    m_modeCombo->addItem("DeepSeek /tools/tts（stella / tide，原始音频）",
                         TTSEngine::apiModeToString(TTSEngine::ApiMode::DeepSeekTts));
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        syncPresetSelection();
        updateModeUi();
    });
    apiLayout->addRow("接口协议:", m_modeCombo);

    m_apiBaseEdit = new QLineEdit;
    m_apiBaseEdit->setMinimumWidth(500);
    m_apiBaseEdit->setPlaceholderText("https://api.xiaomimimo.com/v1 或 http://127.0.0.1:8000/v1");
    connect(m_apiBaseEdit, &QLineEdit::textEdited, this, [this](const QString &) {
        syncPresetSelection();
    });
    apiLayout->addRow("API 地址:", m_apiBaseEdit);

    m_apiKeyEdit = new QLineEdit;
    m_apiKeyEdit->setMinimumWidth(500);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("本地 / 内网地址可留空");
    auto *keyLayout = new QHBoxLayout;
    keyLayout->addWidget(m_apiKeyEdit);
    auto *showKeyCheck = new QCheckBox("显示");
    connect(showKeyCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    keyLayout->addWidget(showKeyCheck);
    apiLayout->addRow("API Key:", keyLayout);

    m_modeHint = new QLabel;
    m_modeHint->setWordWrap(true);
    QPalette hintPalette = m_modeHint->palette();
    hintPalette.setColor(QPalette::WindowText, Qt::gray);
    m_modeHint->setPalette(hintPalette);
    apiLayout->addRow(QString(), m_modeHint);

    return apiGroup;
}

QWidget *MainWindow::buildTtsTab() {
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *paramGroup = new QGroupBox("语音参数");
    auto *paramLayout = new QGridLayout(paramGroup);

    paramLayout->addWidget(new QLabel("模型:"), 0, 0);
    m_modelCombo = new QComboBox;
    m_modelCombo->setEditable(true);          // 本地服务的模型名可以手填
    m_modelCombo->setMinimumWidth(220);
    m_modelCombo->setInsertPolicy(QComboBox::NoInsert);
    paramLayout->addWidget(m_modelCombo, 0, 1);

    m_refreshModelsBtn = new QPushButton("刷新");
    connect(m_refreshModelsBtn, &QPushButton::clicked, this, &MainWindow::onFetchModels);
    paramLayout->addWidget(m_refreshModelsBtn, 0, 2);

    m_onlyTtsCheck = new QCheckBox("仅 TTS");
    m_onlyTtsCheck->setToolTip("勾选后只显示模型名里带 \"tts\" 的条目。\n"
                               "本地服务的模型名常常不含 tts，可取消勾选。");
    connect(m_onlyTtsCheck, &QCheckBox::toggled, this, [this](bool) { applyModelFilter(); });
    paramLayout->addWidget(m_onlyTtsCheck, 0, 3);

    paramLayout->addWidget(new QLabel("声音:"), 0, 4);
    m_voiceCombo = new QComboBox;
    m_voiceCombo->setEditable(true);          // 本地服务的音色名可以手填
    m_voiceCombo->setInsertPolicy(QComboBox::NoInsert);
    paramLayout->addWidget(m_voiceCombo, 0, 5);

    auto *voiceSettingsBtn = new QPushButton("设置");
    connect(voiceSettingsBtn, &QPushButton::clicked, this, &MainWindow::onVoiceSettings);
    paramLayout->addWidget(voiceSettingsBtn, 0, 6);

    paramLayout->addWidget(new QLabel("格式:"), 1, 0);
    m_formatCombo = new QComboBox;
    m_formatCombo->addItem("wav（推荐）", "wav");
    m_formatCombo->addItem("pcm16（本地自动封装为 WAV）", "pcm16");
    m_formatCombo->setToolTip("pcm16 是裸 PCM，播放器无法直接识别；\n"
                              "本工具会在本地补上 WAV 头，所以保存的文件仍是 .wav。");
    paramLayout->addWidget(m_formatCombo, 1, 1, 1, 2);

    layout->addWidget(paramGroup);

    auto *styleGroup = new QGroupBox("风格控制 (可选，自然语言描述语气风格)");
    auto *styleLayout = new QVBoxLayout(styleGroup);
    m_styleEdit = new QPlainTextEdit;
    m_styleEdit->setMaximumHeight(80);
    m_styleEdit->setPlaceholderText("留空则不发送。MiMo 走 user 消息，OpenAI 协议走 instructions 字段。");
    styleLayout->addWidget(m_styleEdit);
    layout->addWidget(styleGroup);

    auto *textGroup = new QGroupBox("合成文本");
    auto *textLayout = new QVBoxLayout(textGroup);
    m_textEdit = new QPlainTextEdit;
    m_textEdit->setMinimumHeight(100);
    m_textEdit->setPlainText("生活就像海洋，只有意志坚强的人才能到达彼岸\nThis is an apple. I like apples. Apples are good for our health.");
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
    m_cloneTextEdit->setPlainText("生活就像海洋，只有意志坚强的人才能到达彼岸\nThis is an apple. I like apples. Apples are good for our health.");
    textLayout->addWidget(m_cloneTextEdit);
    layout->addWidget(textGroup, 1);

    auto *hintLabel = new QLabel("提示: 音色复刻依赖 MiMo 的 mimo-v2.5-tts-voiceclone 模型，"
                                 "需要把接口协议切回 /chat/completions。");
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

    const int modeIdx = m_modeCombo->findData(m_config->apiMode());
    m_modeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);

    m_onlyTtsCheck->setChecked(m_config->onlyTtsModels());
    applyModelFilter();

    const QString model = m_config->model();
    if (!model.isEmpty()) {
        const int idx = m_modelCombo->findText(model);
        if (idx >= 0)
            m_modelCombo->setCurrentIndex(idx);
        else
            m_modelCombo->setEditText(model);
    }

    m_voiceCombo->clear();
    m_voiceCombo->addItems(m_config->voices());
    const QString voice = m_config->voice();
    if (!voice.isEmpty()) {
        const int idx = m_voiceCombo->findText(voice);
        if (idx >= 0)
            m_voiceCombo->setCurrentIndex(idx);
        else
            m_voiceCombo->setEditText(voice);
    }

    const int formatIdx = m_formatCombo->findData(m_config->outputFormat());
    if (formatIdx >= 0)
        m_formatCombo->setCurrentIndex(formatIdx);

    syncPresetSelection();
    updateModeUi();
}

void MainWindow::updateModeUi() {
    if (!m_modeHint || !m_modeCombo)
        return;

    const TTSEngine::ApiMode mode =
        TTSEngine::apiModeFromString(m_modeCombo->currentData().toString());
    const bool deepseek = mode == TTSEngine::ApiMode::DeepSeekTts;

    // DeepSeek 的 TTS 接口没有 model、没有风格参数、也没有可拉取的 TTS 模型列表，
    // 把这些不生效的控件直接禁用，免得用户以为填了有用。
    if (m_modelCombo) m_modelCombo->setEnabled(!deepseek);
    if (m_refreshModelsBtn) m_refreshModelsBtn->setEnabled(!deepseek);
    if (m_onlyTtsCheck) m_onlyTtsCheck->setEnabled(!deepseek);
    if (m_styleEdit) {
        m_styleEdit->setEnabled(!deepseek);
        m_styleEdit->setPlaceholderText(deepseek
            ? QStringLiteral("DeepSeek TTS 不支持风格控制")
            : QStringLiteral("留空则不发送。MiMo 走 user 消息，OpenAI 协议走 instructions 字段。"));
    }
    // 切换协议时把该协议的音色补进下拉，否则用户无从选择。
    if (deepseek && m_voiceCombo) {
        for (const char *voice : {"stella", "tide"}) {
            if (m_voiceCombo->findText(QLatin1String(voice)) < 0)
                m_voiceCombo->addItem(QLatin1String(voice));
        }
    }

    switch (mode) {
    case TTSEngine::ApiMode::OpenAiSpeech:
        m_modeHint->setText(QStringLiteral(
            "本地服务：填 http://127.0.0.1:端口/v1，Key 可留空，点\"刷新\"可读取模型列表。"));
        break;
    case TTSEngine::ApiMode::DeepSeekTts:
        m_modeHint->setText(QStringLiteral(
            "DeepSeek 工具 API：地址 https://api.deepseek.com/v1，音色 stella(女)/tide(男)，"
            "语种自动识别。汉字算 2 个计费字符，单次上限 10000 计费字符 / 64 KiB。"));
        break;
    case TTSEngine::ApiMode::ChatCompletions:
        m_modeHint->setText(QStringLiteral("云端 MiMo 服务：需要填写 API Key。"));
        break;
    }
}

QString MainWindow::currentFormat() const {
    const QString format = m_formatCombo->currentData().toString();
    return format.isEmpty() ? QStringLiteral("wav") : format;
}

TTSEngine::Request MainWindow::currentRequest(const QString &text, const QString &format) const {
    TTSEngine::Request request;
    request.text = text;
    request.apiBase = m_apiBaseEdit->text().trimmed();
    request.apiKey = m_apiKeyEdit->text().trimmed();
    request.model = m_modelCombo->currentText().trimmed();
    request.voice = m_voiceCombo->currentText().trimmed();
    request.format = format;
    request.stylePrompt = m_styleEdit->toPlainText().trimmed();
    request.mode = TTSEngine::apiModeFromString(m_modeCombo->currentData().toString());
    return request;
}

void MainWindow::setBusy(Tab tab, bool busy) {
    // 引擎同一时刻只处理一个请求，两侧的播放/保存都一并锁住。
    m_ttsPlayBtn->setEnabled(!busy);
    m_clonePlayBtn->setEnabled(!busy);
    m_ttsSaveBtn->setEnabled(!busy);
    m_cloneSaveBtn->setEnabled(!busy);
    m_ttsStopBtn->setEnabled(busy && tab == Tab::Tts);
    m_cloneStopBtn->setEnabled(busy && tab == Tab::Clone);
}

void MainWindow::applyPreset(int index) {
    const QList<ApiPreset> &presets = apiPresets();
    if (index < 0 || index >= presets.size())
        return;

    const ApiPreset &preset = presets.at(index);
    if (preset.base.isEmpty())
        return;   // "自定义"：保留用户已填内容

    m_apiBaseEdit->setText(preset.base);

    {
        QSignalBlocker blocker(m_modeCombo);
        const int modeIdx = m_modeCombo->findData(TTSEngine::apiModeToString(preset.mode));
        if (modeIdx >= 0)
            m_modeCombo->setCurrentIndex(modeIdx);
    }

    if (!preset.model.isEmpty()) {
        if (m_modelCombo->findText(preset.model) < 0)
            m_modelCombo->addItem(preset.model);
        m_modelCombo->setEditText(preset.model);
    }
    if (!preset.voice.isEmpty()) {
        if (m_voiceCombo->findText(preset.voice) < 0)
            m_voiceCombo->addItem(preset.voice);
        m_voiceCombo->setEditText(preset.voice);
    }

    updateModeUi();
    m_statusLabel->setText(QString("已切换到预设: %1").arg(preset.name));
}

void MainWindow::onPresetChanged(int index) {
    applyPreset(index);
}

void MainWindow::syncPresetSelection() {
    if (!m_presetCombo)
        return;

    const QString base = m_apiBaseEdit->text().trimmed();
    const QString mode = m_modeCombo->currentData().toString();
    const QList<ApiPreset> &presets = apiPresets();

    int match = presets.size() - 1;   // 默认落到"自定义"
    for (int i = 0; i < presets.size() - 1; ++i) {
        if (presets.at(i).base == base && TTSEngine::apiModeToString(presets.at(i).mode) == mode) {
            match = i;
            break;
        }
    }

    QSignalBlocker blocker(m_presetCombo);
    m_presetCombo->setCurrentIndex(match);
}

void MainWindow::applyModelFilter() {
    QStringList models = m_allModels;
    if (models.isEmpty())
        models = TTSEngine::defaultModels();

    if (m_onlyTtsCheck->isChecked()) {
        QStringList filtered;
        for (const QString &model : models) {
            if (model.contains("tts", Qt::CaseInsensitive))
                filtered.append(model);
        }
        if (!filtered.isEmpty())
            models = filtered;
    }

    const QString current = m_modelCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItems(models);

    const int idx = m_modelCombo->findText(current);
    if (idx >= 0)
        m_modelCombo->setCurrentIndex(idx);
    else if (!current.isEmpty())
        m_modelCombo->setEditText(current);
}

void MainWindow::onModelsReceived(const QStringList &models) {
    m_allModels = models;
    applyModelFilter();
    m_statusLabel->setText(QString("模型列表已更新（%1 个）").arg(m_modelCombo->count()));
}

bool MainWindow::checkReady(const QString &text) {
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要合成的文本");
        return false;
    }
    const QString base = m_apiBaseEdit->text().trimmed();
    if (base.isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写 API 地址");
        return false;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty() && !TTSEngine::isLocalEndpoint(base)) {
        QMessageBox::warning(this, "提示",
                             "请填写 API Key（本地 / 内网地址可以留空）");
        return false;
    }
    // DeepSeek TTS 没有 model 参数，不要求填模型名。
    const TTSEngine::ApiMode mode =
        TTSEngine::apiModeFromString(m_modeCombo->currentData().toString());
    if (mode != TTSEngine::ApiMode::DeepSeekTts
        && m_modelCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择或填写模型名");
        return false;
    }
    return true;
}

bool MainWindow::checkCloneReady(const QString &text) {
    if (!checkReady(text))
        return false;
    if (TTSEngine::apiModeFromString(m_modeCombo->currentData().toString())
        != TTSEngine::ApiMode::ChatCompletions) {
        QMessageBox::warning(this, "提示",
                             "音色复刻需要在接口协议里选择 MiMo /chat/completions");
        return false;
    }
    if (m_audioFileEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择参考音频文件");
        return false;
    }
    return true;
}

void MainWindow::onTtsPlay() {
    const QString text = m_textEdit->toPlainText().trimmed();
    if (!checkReady(text))
        return;

    m_pendingAction = PendingAction::Play;
    m_pendingTab = Tab::Tts;
    setBusy(Tab::Tts, true);
    m_statusLabel->setText("正在合成语音...");

    m_engine->synthesize(currentRequest(text, currentFormat()));
}

void MainWindow::onTtsSave() {
    const QString text = m_textEdit->toPlainText().trimmed();
    if (!checkReady(text))
        return;

    const QString suggested = QDir::homePath() + "/tts-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".wav";
    const QString filePath = QFileDialog::getSaveFileName(this, "保存音频", suggested,
                                                          "WAV 音频 (*.wav);;所有文件 (*.*)");
    if (filePath.isEmpty())
        return;

    m_pendingAction = PendingAction::Save;
    m_pendingTab = Tab::Tts;
    m_pendingSavePath = filePath;
    setBusy(Tab::Tts, true);
    m_statusLabel->setText("正在合成并保存...");

    m_engine->synthesize(currentRequest(text, currentFormat()));
}

void MainWindow::onTtsStop() {
    m_pendingAction = PendingAction::None;
    m_pendingTab = Tab::Tts;
    m_engine->stop();
    setBusy(Tab::Tts, false);
    m_statusLabel->setText("已停止");
}

void MainWindow::onClonePlay() {
    const QString text = m_cloneTextEdit->toPlainText().trimmed();
    if (!checkCloneReady(text))
        return;

    m_pendingAction = PendingAction::Play;
    m_pendingTab = Tab::Clone;
    setBusy(Tab::Clone, true);
    m_statusLabel->setText("正在合成语音...");

    TTSEngine::Request request = currentRequest(text, currentFormat());
    request.stylePrompt.clear();   // 风格框在 TTS 页，复刻页不隐式复用
    m_engine->cloneVoice(request, m_audioFileEdit->text().trimmed());
}

void MainWindow::onCloneSave() {
    const QString text = m_cloneTextEdit->toPlainText().trimmed();
    if (!checkCloneReady(text))
        return;

    const QString suggested = QDir::homePath() + "/tts-clone-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".wav";
    const QString filePath = QFileDialog::getSaveFileName(this, "保存音频", suggested,
                                                          "WAV 音频 (*.wav);;所有文件 (*.*)");
    if (filePath.isEmpty())
        return;

    m_pendingAction = PendingAction::Save;
    m_pendingTab = Tab::Clone;
    m_pendingSavePath = filePath;
    setBusy(Tab::Clone, true);
    m_statusLabel->setText("正在合成并保存...");

    TTSEngine::Request request = currentRequest(text, currentFormat());
    request.stylePrompt.clear();
    m_engine->cloneVoice(request, m_audioFileEdit->text().trimmed());
}

void MainWindow::onCloneStop() {
    m_pendingAction = PendingAction::None;
    m_pendingTab = Tab::Clone;
    m_engine->stop();
    setBusy(Tab::Clone, false);
    m_statusLabel->setText("已停止");
}

void MainWindow::onSynthesisFinished(const QString &filePath) {
    const PendingAction action = m_pendingAction;
    m_pendingAction = PendingAction::None;

    // 保存：只落盘，不播放
    if (action == PendingAction::Save) {
        const QString target = m_pendingSavePath;
        m_pendingSavePath.clear();

        QFile::remove(target);
        const bool ok = QFile::copy(filePath, target);
        setBusy(m_pendingTab, false);

        if (ok) {
            m_statusLabel->setText("已保存: " + target);
            QMessageBox::information(this, "成功", "音频已保存到:\n" + target);
        } else {
            m_statusLabel->setText("保存失败");
            QMessageBox::critical(this, "错误", "保存失败，请检查目标路径是否可写");
        }
        return;
    }

    // 播放：按钮保持锁定，等 playbackFinished / 用户停止再解锁
    m_statusLabel->setText("正在播放...");
    m_engine->play(filePath);
}

void MainWindow::onSynthesisError(const QString &error) {
    setBusy(m_pendingTab, false);
    m_pendingAction = PendingAction::None;
    m_pendingSavePath.clear();
    m_statusLabel->setText("出错");
    QMessageBox::critical(this, "错误", error);
}

void MainWindow::onSynthesisCancelled() {
    setBusy(m_pendingTab, false);
    m_pendingAction = PendingAction::None;
    m_pendingSavePath.clear();
    m_statusLabel->setText("已停止");
}

void MainWindow::onPlaybackFinished() {
    setBusy(m_pendingTab, false);
    m_statusLabel->setText("播放完成");
}

void MainWindow::onSaveConfig() {
    m_config->setApiBase(m_apiBaseEdit->text().trimmed());
    m_config->setApiKey(m_apiKeyEdit->text().trimmed());
    m_config->setApiMode(m_modeCombo->currentData().toString());
    m_config->setModel(m_modelCombo->currentText().trimmed());
    m_config->setVoice(m_voiceCombo->currentText().trimmed());
    m_config->setOutputFormat(currentFormat());
    m_config->setOnlyTtsModels(m_onlyTtsCheck->isChecked());
    m_config->save();
    QMessageBox::information(this, "成功", "配置已保存");
}

void MainWindow::onFetchModels() {
    const QString apiBase = m_apiBaseEdit->text().trimmed();
    const QString apiKey = m_apiKeyEdit->text().trimmed();

    if (apiBase.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先填写 API 地址");
        return;
    }
    if (apiKey.isEmpty() && !TTSEngine::isLocalEndpoint(apiBase)) {
        QMessageBox::warning(this, "提示", "请先输入 API Key（本地 / 内网地址可以留空）");
        return;
    }

    m_statusLabel->setText("正在获取模型列表...");
    m_engine->fetchModels(apiBase, apiKey,
                          TTSEngine::apiModeFromString(m_modeCombo->currentData().toString()));
}

void MainWindow::onVoiceSettings() {
    VoiceSettingsDialog dlg(m_config.get(), this);
    dlg.exec();

    const QString current = m_voiceCombo->currentText();
    m_voiceCombo->clear();
    m_voiceCombo->addItems(m_config->voices());
    const int idx = m_voiceCombo->findText(current);
    if (idx >= 0)
        m_voiceCombo->setCurrentIndex(idx);
    else if (!current.isEmpty())
        m_voiceCombo->setEditText(current);
}

void MainWindow::onSelectAudioFile() {
    const QString filePath = QFileDialog::getOpenFileName(this, "选择参考音频文件", "",
        "音频文件 (*.wav *.mp3);;所有文件 (*.*)");
    if (!filePath.isEmpty())
        m_audioFileEdit->setText(filePath);
}
