#pragma once

#include <QWidget>

class QPlainTextEdit;
class QToolButton;
class QLabel;

class OutputPanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutputPanel(QWidget *parent = nullptr);

    void clear();
    void append(const QString &text);
    void expand();
    void collapse();
    void setBusy(bool busy);
    bool isExpanded() const;

signals:
    void cancelRequested();

private:
    void toggle();

    QToolButton *m_toggle = nullptr;
    QToolButton *m_cancel = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_view = nullptr;
    QWidget *m_body = nullptr;
    bool m_expanded = false;
};
