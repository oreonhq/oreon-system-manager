#include "ProcessRunner.h"

#include <QProcessEnvironment>

ProcessRunner::ProcessRunner(QObject *parent)
    : QObject(parent)
{
}

ProcessRunner::~ProcessRunner()
{
    cancel();
}

bool ProcessRunner::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void ProcessRunner::cancel()
{
    if (!m_proc)
        return;
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(1500))
            m_proc->kill();
    }
    m_proc->deleteLater();
    m_proc = nullptr;
}

void ProcessRunner::start(const QString &program, const QStringList &args)
{
    cancel();
    m_proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    m_proc->setProcessEnvironment(env);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString text = QString::fromUtf8(m_proc->readAllStandardOutput());
        if (!text.isEmpty())
            emit outputReceived(text, true);
    });
    connect(m_proc, &QProcess::readyReadStandardError, this, [this]() {
        const QString text = QString::fromUtf8(m_proc->readAllStandardError());
        if (!text.isEmpty())
            emit outputReceived(text, false);
    });
    connect(m_proc, &QProcess::started, this, &ProcessRunner::started);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st) {
                emit finished(code, st);
            });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            emit failedToStart(m_proc ? m_proc->errorString() : QStringLiteral("failed to start"));
    });

    m_proc->start(program, args);
}

void ProcessRunner::startPkexec(const QStringList &args)
{
    start(QStringLiteral("pkexec"), args);
}

bool ProcessRunner::isDnfNoise(const QString &line)
{
    const QString t = line.trimmed();
    if (t.isEmpty() || t.startsWith(QLatin1Char('=')))
        return true;
    if (t.startsWith(QLatin1String("Last metadata")))
        return true;
    if (t.startsWith(QLatin1String("Matched fields")))
        return true;
    if (t.startsWith(QLatin1String("Updating and loading")))
        return true;
    if (t.startsWith(QLatin1String("Repositories loaded")))
        return true;
    if (t.startsWith(QLatin1String("No matching packages")))
        return true;
    if (t.compare(QLatin1String("available packages"), Qt::CaseInsensitive) == 0)
        return true;
    if (t.compare(QLatin1String("installed packages"), Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

bool ProcessRunner::looksLikeNevra(const QString &token)
{
    const int dot = token.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot >= token.size() - 1)
        return false;
    const QString arch = token.mid(dot + 1);
    for (QChar c : arch) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_'))
            return false;
    }
    return true;
}

QStringList ProcessRunner::parseDnfSearchOutput(const QString &data)
{
    QStringList out;
    for (const QString &raw : data.split(QLatin1Char('\n'))) {
        const QString l = raw.trimmed();
        if (isDnfNoise(l))
            continue;
        if (l.contains(QLatin1String(" : "))) {
            const auto parts = l.split(QLatin1String(" : "));
            out << (parts.value(0).trimmed() + QLatin1String(" : ") + parts.value(1).trimmed());
        } else if (l.contains(QLatin1Char('\t'))) {
            const auto parts = l.split(QLatin1Char('\t'));
            out << (parts.value(0).trimmed() + QLatin1String(" : ") + parts.value(1).trimmed());
        } else {
            const QString first = l.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
            if (looksLikeNevra(first))
                out << l;
        }
    }
    return out;
}

QStringList ProcessRunner::parseDnfListOutput(const QString &data)
{
    QStringList out;
    for (const QString &raw : data.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (isDnfNoise(line))
            continue;
        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() >= 3 && looksLikeNevra(fields.at(0)))
            out << line;
    }
    return out;
}

QStringList ProcessRunner::parseListOutput(const QString &data)
{
    QStringList out;
    for (const QString &raw : data.split(QLatin1Char('\n'))) {
        const QString l = raw.trimmed();
        if (l.isEmpty())
            continue;
        if (l.startsWith(QLatin1String("repo id")) || l.startsWith(QLatin1String("Repo-id")))
            continue;
        if (l.startsWith(QLatin1String("CONTAINER")) || l.startsWith(QLatin1String("ID")))
            continue;
        if (l.startsWith(QLatin1String("Updating")) || l.startsWith(QLatin1String("Repositories loaded")))
            continue;
        out << l;
    }
    return out;
}

QStringList ProcessRunner::parseDockerPsOutput(const QString &data)
{
    QStringList out;
    for (const QString &raw : data.split(QLatin1Char('\n'))) {
        const QString l = raw.trimmed();
        if (!l.isEmpty())
            out << l;
    }
    return out;
}

QStringList ProcessRunner::parseDistroboxListOutput(const QString &data)
{
    QStringList out;
    for (const QString &raw : data.split(QLatin1Char('\n'))) {
        const QString l = raw.trimmed();
        if (l.isEmpty() || l.startsWith(QLatin1String("ID")) || l.startsWith(QLatin1String("---")))
            continue;
        out << l;
    }
    return out;
}

QString ProcessRunner::extractPackageName(const QString &line)
{
    QString first = line.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
    if (first.contains(QLatin1String(" : ")))
        first = first.section(QLatin1String(" : "), 0, 0);
    // strip arch: name.arch -> name
    const int dot = first.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        return first.left(dot);
    return first;
}

QString ProcessRunner::extractRepoId(const QString &line)
{
    return line.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
}

bool ProcessRunner::repoIsEnabled(const QString &line)
{
    const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return !parts.isEmpty() && parts.last() == QLatin1String("enabled");
}

QString ProcessRunner::extractDockerName(const QString &line)
{
    return line.split(QLatin1Char('\t')).value(0).trimmed();
}

QString ProcessRunner::extractDistroboxName(const QString &line)
{
    if (line.contains(QLatin1Char('|'))) {
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() >= 2)
            return parts.at(1).trimmed();
    }
    return line.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
}

QString ProcessRunner::formatResult(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit)
        return QStringLiteral("\n[crashed]\n");
    if (exitCode == 0)
        return QStringLiteral("\n[done]\n");
    if (exitCode == 126 || exitCode == 127)
        return QStringLiteral("\n[cancelled or not found]\n");
    return QStringLiteral("\n[exit %1]\n").arg(exitCode);
}
