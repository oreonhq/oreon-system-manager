#include "MainWindow.h"
#include "Settings.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Oreon System Manager"));
    app.setApplicationVersion(QStringLiteral("1.0.3"));
    app.setOrganizationName(QStringLiteral("org.oreon"));
    app.setOrganizationDomain(QStringLiteral("oreonhq.com"));
    app.setDesktopFileName(QStringLiteral("oreon-system-manager"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Oreon System Manager: packages, repos, containers, drivers"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const QStringList keys = QStyleFactory::keys();
    if (keys.contains(QStringLiteral("Breeze"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("Breeze"));
    else if (keys.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("Fusion"));

    Settings::instance().applyThemeToApp();

    app.setStyleSheet(app.styleSheet() + QStringLiteral(
        "QLineEdit, QComboBox { padding: 4px 8px; }"
        "QCheckBox { spacing: 6px; }"
        "QTabWidget::pane { border-top: 1px solid palette(mid); }"
    ));

    MainWindow w;
    w.show();
    return app.exec();
}
