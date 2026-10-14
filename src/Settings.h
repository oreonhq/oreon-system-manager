#pragma once

#include <QObject>
#include <QString>

class Settings : public QObject
{
    Q_OBJECT
public:
    enum Theme { BreezeLight = 0, BreezeDark = 1 };

    static Settings &instance();

    Theme theme() const;
    void setTheme(Theme t);

    bool confirmBeforeRemove() const;
    void setConfirmBeforeRemove(bool v);

    QString terminalCommand() const;
    void setTerminalCommand(const QString &cmd);

    void applyThemeToApp();
    void load();
    void save();

signals:
    void themeChanged(Theme t);
    void settingsChanged();

private:
    Settings();
    Theme m_theme = BreezeLight;
    bool m_confirmRemove = true;
    QString m_terminal = QStringLiteral("konsole");
};
