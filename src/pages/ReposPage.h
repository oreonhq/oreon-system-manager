#pragma once

#include <QProcess>
#include <QWidget>

class QTableWidget;
class QPushButton;
class QToolButton;
class OutputPanel;
class ProcessRunner;

class ReposPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReposPage(QWidget *parent = nullptr);
    void refresh();

signals:
    void activity(const QString &message);

private:
    void loadRepos();
    void toggleSelected();
    void enableSelected();
    void disableSelected();
    void addRepo();
    void removeRepo();
    void setBusy(bool busy);
    void onOutput(const QString &text, bool isStdout);
    void onFinished(int code, QProcess::ExitStatus st);
    QString selectedRepoId() const;
    bool selectedEnabled() const;

    QTableWidget *m_table = nullptr;
    QPushButton *m_toggleBtn = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QToolButton *m_moreBtn = nullptr;
    OutputPanel *m_output = nullptr;
    ProcessRunner *m_runner = nullptr;
    QString m_buffer;
    enum class Mode { List, Action } m_mode = Mode::List;
    bool m_triedFallback = false;
    bool m_triedSetopt = false;
    QString m_pendingRepoId;
    bool m_pendingEnable = false;
};
