#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

class ProcessRunner : public QObject
{
    Q_OBJECT
public:
    explicit ProcessRunner(QObject *parent = nullptr);
    ~ProcessRunner() override;

    bool isRunning() const;
    void start(const QString &program, const QStringList &args);
    void startPkexec(const QStringList &args);
    void cancel();

    // parsing helpers (ported from process.rs)
    static bool isDnfNoise(const QString &line);
    static bool looksLikeNevra(const QString &token);
    static QStringList parseDnfSearchOutput(const QString &data);
    static QStringList parseDnfListOutput(const QString &data);
    static QStringList parseListOutput(const QString &data);
    static QStringList parseDockerPsOutput(const QString &data);
    static QStringList parseDistroboxListOutput(const QString &data);
    static QString extractPackageName(const QString &line);
    static QString extractRepoId(const QString &line);
    static bool repoIsEnabled(const QString &line);
    static QString extractDockerName(const QString &line);
    static QString extractDistroboxName(const QString &line);
    static QString formatResult(int exitCode, QProcess::ExitStatus status);

signals:
    void outputReceived(const QString &text, bool isStdout);
    void finished(int exitCode, QProcess::ExitStatus status);
    void started();
    void failedToStart(const QString &error);

private:
    QProcess *m_proc = nullptr;
};
