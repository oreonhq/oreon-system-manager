#include "OutputPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto *header = new QHBoxLayout;
    m_toggle = new QToolButton(this);
    m_toggle->setText(QStringLiteral("Details"));
    m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggle->setArrowType(Qt::RightArrow);
    m_toggle->setAutoRaise(true);
    connect(m_toggle, &QToolButton::clicked, this, &OutputPanel::toggle);

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color: gray; font-size: 12px;"));

    m_cancel = new QToolButton(this);
    m_cancel->setText(QStringLiteral("Cancel"));
    m_cancel->setVisible(false);
    connect(m_cancel, &QToolButton::clicked, this, &OutputPanel::cancelRequested);

    header->addWidget(m_toggle);
    header->addWidget(m_status, 1);
    header->addWidget(m_cancel);
    root->addLayout(header);

    m_body = new QWidget(this);
    auto *bodyLay = new QVBoxLayout(m_body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    m_view = new QPlainTextEdit(m_body);
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(20000);
    m_view->setMaximumHeight(160);
    m_view->setMinimumHeight(80);
    m_view->setFont(QFont(QStringLiteral("monospace")));
    bodyLay->addWidget(m_view);
    m_body->setVisible(false);
    root->addWidget(m_body);
}

void OutputPanel::clear()
{
    m_view->clear();
}

void OutputPanel::append(const QString &text)
{
    m_view->moveCursor(QTextCursor::End);
    m_view->insertPlainText(text);
    m_view->moveCursor(QTextCursor::End);
}

void OutputPanel::expand()
{
    if (m_expanded)
        return;
    m_expanded = true;
    m_body->setVisible(true);
    m_toggle->setArrowType(Qt::DownArrow);
}

void OutputPanel::collapse()
{
    if (!m_expanded)
        return;
    m_expanded = false;
    m_body->setVisible(false);
    m_toggle->setArrowType(Qt::RightArrow);
}

void OutputPanel::setBusy(bool busy)
{
    m_cancel->setVisible(busy);
    m_status->setText(busy ? QStringLiteral("running…") : QString());
}

bool OutputPanel::isExpanded() const { return m_expanded; }

void OutputPanel::toggle()
{
    if (m_expanded)
        collapse();
    else
        expand();
}
