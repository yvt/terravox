#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
    QApplication::setStyle("fusion");

    QApplication a(argc, argv);

    a.setApplicationName("Terravox");
    a.setOrganizationDomain("terraworks.org");
    a.setOrganizationName("Terraworks");

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53,53,53));
    darkPalette.setColor(QPalette::WindowText, QColor(210, 210, 210));
    darkPalette.setColor(QPalette::Base, QColor(25,25,25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(210, 210, 210));
    darkPalette.setColor(QPalette::ToolTipText, QColor(210, 210, 210));
    darkPalette.setColor(QPalette::Text, QColor(210, 210, 210));
    darkPalette.setColor(QPalette::Button, QColor(53,53,53));
    darkPalette.setColor(QPalette::ButtonText, QColor(210, 210, 210));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    a.setPalette(darkPalette);

    a.setStyleSheet("*{font-size: 11px;} QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&qtTranslator);

    QTranslator myappTranslator;
    QString dir;
#ifdef Q_OS_DARWIN
    dir = QApplication::applicationDirPath() + "/../Resources";
#endif
    myappTranslator.load(QLocale::system(), "terravox_", QString(), dir);
    a.installTranslator(&myappTranslator);

    MainWindow *w = new MainWindow();
    w->show();

    return a.exec();
}
