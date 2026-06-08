#pragma once

#include <QWidget>

class QListWidget;
class QTextEdit;
class QPushButton;
class QTabWidget;
class QProcess;

class ContainerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ContainerPage(QWidget *parent = nullptr);

private slots:
    void refreshDocker();
    void refreshDistrobox();
    void onDockerAction(const QString &action);
    void onDistroboxAction(const QString &action);

private:
    // Each backend gets its own process so concurrent refreshes don't race.
    void runDocker(const QStringList &args);
    void runDistrobox(const QStringList &args);

    QTabWidget  *m_tabs;
    QListWidget *m_dockerList;
    QListWidget *m_distroboxList;
    QTextEdit   *m_output;
    QProcess    *m_dockerProcess;
    QProcess    *m_distroboxProcess;
};
