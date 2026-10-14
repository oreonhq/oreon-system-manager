#include "Settings.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

Settings::Settings()
{
    load();
}

void Settings::load()
{
    QSettings s(QStringLiteral("org.oreon"), QStringLiteral("SystemManager"));
    m_theme = static_cast<Theme>(s.value(QStringLiteral("theme"), 0).toInt());
    m_confirmRemove = s.value(QStringLiteral("confirmBeforeRemove"), true).toBool();
    m_terminal = s.value(QStringLiteral("terminalCommand"), QStringLiteral("konsole")).toString();
}

void Settings::save()
{
    QSettings s(QStringLiteral("org.oreon"), QStringLiteral("SystemManager"));
    s.setValue(QStringLiteral("theme"), static_cast<int>(m_theme));
    s.setValue(QStringLiteral("confirmBeforeRemove"), m_confirmRemove);
    s.setValue(QStringLiteral("terminalCommand"), m_terminal);
}

Settings::Theme Settings::theme() const { return m_theme; }

void Settings::setTheme(Theme t)
{
    if (m_theme == t)
        return;
    m_theme = t;
    save();
    applyThemeToApp();
    emit themeChanged(t);
    emit settingsChanged();
}

bool Settings::confirmBeforeRemove() const { return m_confirmRemove; }

void Settings::setConfirmBeforeRemove(bool v)
{
    if (m_confirmRemove == v)
        return;
    m_confirmRemove = v;
    save();
    emit settingsChanged();
}

QString Settings::terminalCommand() const { return m_terminal; }

void Settings::setTerminalCommand(const QString &cmd)
{
    if (m_terminal == cmd)
        return;
    m_terminal = cmd;
    save();
    emit settingsChanged();
}

static QPalette breezeLightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(252, 252, 252));
    p.setColor(QPalette::WindowText, QColor(35, 38, 41));
    p.setColor(QPalette::Base, QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase, QColor(247, 247, 247));
    p.setColor(QPalette::Text, QColor(35, 38, 41));
    p.setColor(QPalette::Button, QColor(252, 252, 252));
    p.setColor(QPalette::ButtonText, QColor(35, 38, 41));
    p.setColor(QPalette::Highlight, QColor(61, 174, 233));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText, QColor(35, 38, 41));
    p.setColor(QPalette::Link, QColor(41, 128, 185));
    p.setColor(QPalette::PlaceholderText, QColor(120, 120, 120));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(140, 140, 140));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(140, 140, 140));
    return p;
}

static QPalette breezeDarkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(42, 46, 50));
    p.setColor(QPalette::WindowText, QColor(252, 252, 252));
    p.setColor(QPalette::Base, QColor(27, 30, 32));
    p.setColor(QPalette::AlternateBase, QColor(35, 38, 41));
    p.setColor(QPalette::Text, QColor(252, 252, 252));
    p.setColor(QPalette::Button, QColor(49, 54, 59));
    p.setColor(QPalette::ButtonText, QColor(252, 252, 252));
    p.setColor(QPalette::Highlight, QColor(61, 174, 233));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, QColor(49, 54, 59));
    p.setColor(QPalette::ToolTipText, QColor(252, 252, 252));
    p.setColor(QPalette::Link, QColor(61, 174, 233));
    p.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    return p;
}

void Settings::applyThemeToApp()
{
    QStringList styles = QStyleFactory::keys();
    if (styles.contains(QStringLiteral("Breeze"), Qt::CaseInsensitive))
        qApp->setStyle(QStringLiteral("Breeze"));
    else if (styles.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        qApp->setStyle(QStringLiteral("Fusion"));

    if (m_theme == BreezeDark)
        qApp->setPalette(breezeDarkPalette());
    else
        qApp->setPalette(breezeLightPalette());
}
