#include "echoclient.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    EchoClient *echoClient = new EchoClient;
    echoClient->open(QUrl("ws://localhost:9000"));
    return app.exec();
}
