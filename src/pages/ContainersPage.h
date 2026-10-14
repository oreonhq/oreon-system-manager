#pragma once

#include <QProcess>
#include <QWidget>

class QTabWidget;
class QTableWidget;
class QPushButton;
class QLabel;
class OutputPanel;
class ProcessRunner;

class ContainersPage : public QWidget
{
    Q_OBJECT
public:
    explicit ContainersPage(QWidget *parent = nullptr);
    void refreshAll();

signals:
    void activity(const QString &message);

private:
    void buildDockerTab();
    void buildDistroboxTab();
    void buildFlatpakTab();

    void refreshDocker();
    void refreshDistrobox();
    void refreshFlatpak();

    void dockerStart();
    void dockerStop();
    void dockerRestart();
    void dockerRemove();
    void dockerCreate();
    void dockerPull();
    void dockerLogs();

    void distroCreate();
    void distroEnter();
    void distroStop();
    void distroRemove();
    void distroExport();

    void flatpakInstall();
    void flatpakUninstall();

    void installDocker();
    void installDistrobox();
    void setBusy(bool busy);
    void onOutput(const QString &text, bool isStdout);
    void onFinished(int code, QProcess::ExitStatus st);
    QString selectedDockerName() const;
    QString selectedDistroName() const;
    QString selectedFlatpakId() const;
    bool toolExists(const QString &name) const;

    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_dockerTable = nullptr;
    QTableWidget *m_distroTable = nullptr;
    QTableWidget *m_flatpakTable = nullptr;
    QLabel *m_dockerHint = nullptr;
    QLabel *m_distroHint = nullptr;
    QLabel *m_flatpakHint = nullptr;
    QWidget *m_dockerEmpty = nullptr;
    QWidget *m_distroEmpty = nullptr;
    OutputPanel *m_output = nullptr;
    ProcessRunner *m_runner = nullptr;
    QString m_buffer;
    enum class Mode { Idle, DockerList, DistroList, FlatpakList, Action } m_mode = Mode::Idle;
};
