#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "TransferController.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    qmlRegisterType<TransferController>("FileTransfer",1,0,"TransferController");

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/qml/App.qml"));
    if(engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}