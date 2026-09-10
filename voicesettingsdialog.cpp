#include "voicesettingsdialog.h"
#include "config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDialogButtonBox>

VoiceSettingsDialog::VoiceSettingsDialog(Config *config, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle("音色设置");
    resize(420, 400);

    auto *mainLayout = new QVBoxLayout(this);

    auto *listLabel = new QLabel("音色列表（常用音色可点\"添加\"保存，本地服务的音色名也可以直接填写）:");
    listLabel->setWordWrap(true);
    mainLayout->addWidget(listLabel);

    m_listWidget = new QListWidget;
    mainLayout->addWidget(m_listWidget, 1);

    auto *inputLayout = new QHBoxLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("输入新音色名称");
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &VoiceSettingsDialog::onAdd);
    inputLayout->addWidget(m_nameEdit);

    auto *addBtn = new QPushButton("添加");
    connect(addBtn, &QPushButton::clicked, this, &VoiceSettingsDialog::onAdd);
    inputLayout->addWidget(addBtn);

    auto *removeBtn = new QPushButton("删除选中");
    connect(removeBtn, &QPushButton::clicked, this, &VoiceSettingsDialog::onRemove);
    inputLayout->addWidget(removeBtn);

    mainLayout->addLayout(inputLayout);

    // 改动只在点"确定"时落盘，取消则完全不动配置。
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("确定");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::accepted, this, &VoiceSettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    loadVoices();
}

QStringList VoiceSettingsDialog::voices() const {
    QStringList result;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        result.append(m_listWidget->item(i)->text());
    }
    return result;
}

void VoiceSettingsDialog::loadVoices() {
    m_listWidget->clear();
    m_listWidget->addItems(m_config->voices());
}

void VoiceSettingsDialog::onAdd() {
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入音色名称");
        return;
    }

    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->text() == name) {
            QMessageBox::warning(this, "提示", "音色已存在");
            return;
        }
    }

    m_listWidget->addItem(name);
    m_listWidget->setCurrentRow(m_listWidget->count() - 1);
    m_nameEdit->clear();
}

void VoiceSettingsDialog::onRemove() {
    auto *item = m_listWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "提示", "请先选择要删除的音色");
        return;
    }

    const QString name = item->text();
    if (QMessageBox::question(this, "确认", "确定删除音色 \"" + name + "\"?") == QMessageBox::Yes)
        delete m_listWidget->takeItem(m_listWidget->row(item));
}

void VoiceSettingsDialog::onAccept() {
    m_config->setVoices(voices());
    m_config->save();
    accept();
}
