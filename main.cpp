#include <QApplication>
#include "mainwindow.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("TTS");
    app.setApplicationDisplayName("TTS 文本朗读工具");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("tts");

    MainWindow w;
    w.show();

    return app.exec();
}
