#include "diagnostic_transport.hpp"
#include <spentit/diagnostics.hpp>
#include <QCoreApplication>
#include <QLocalSocket>
#include <QFileInfo>
#include <QJsonDocument>
#include <QThread>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <QProcess>
#include <QDir>
static void check(bool ok){
    if(!ok){
        std::cerr
        << "Transport assertion failed\n";
        std::exit(1);
    }
}
static QByteArray request(const QString& endpoint,QByteArray payload){
    QLocalSocket socket;
    socket.connectToServer(endpoint);
    check(socket.waitForConnected(1000));
    socket.write(payload);
    check(socket.waitForBytesWritten(1000));
    QByteArray out;
    while(!out.endsWith('\n')){
        if(!socket.bytesAvailable() && !socket.waitForReadyRead(2000))break;
        out+=socket.readAll();
        check(out.size()<4*1024*1024);
    }
    return out;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    auto& c=spentit::diagnostics::Collector::instance();
    check(c.enable());
    QString endpoint;
    if(app.arguments().contains("--child")){
        DiagnosticTransport child;
        check(child.start());
        std::cout
        << child.endpoint().toStdString()
        << std::endl;
        return app.exec();
    }
    {
        DiagnosticTransport one,two;
        check(one.start());
        check(two.start());
        check(one.endpoint()!=two.endpoint());
        endpoint=one.endpoint();
        check(!(QFileInfo(QFileInfo(endpoint).absolutePath()).permissions() & (QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther)));
        // No GUI event processing: these blocking reads exercise the independent server.
        const auto status=request(endpoint,"{\"method\":\"status\"}\n");
        check(QJsonDocument::fromJson(status).isObject());
        check(!status.contains("CANARY"));
        for(const auto& invalid: {
            QByteArray("{\"method\":\"sql\",\"text\":\"CANARY-private-payee-password\"}\n"),QByteArray("{\"method\":\"status\",\"path\":\"CANARY\"}\n"),QByteArray(1025,'x'),QByteArray("{\"method\":\"events\",\"after\":-1}\n")
        }){
            const auto out=request(endpoint,invalid);
            check(out.contains("invalid_request"));
            check(!out.contains("CANARY"));
        }
        std::vector<std::unique_ptr<QLocalSocket>> stalled;
        for(int i=0;i<8;++i){
            auto socket=std::make_unique<QLocalSocket>();
            socket->connectToServer(endpoint);
            socket->waitForConnected(1000);
            socket->write("{");
            stalled.push_back(std::move(socket));
        }
        for(int i=0;i<10000;++i)c.event({});
        check(c.snapshot().sequence>=10000);
        stalled.clear();
        QThread::msleep(20);
        check(request(endpoint,"{\"method\":\"events\",\"after\":\"0\"}\n").contains("cursor_stale"));
    }
    check(!QFileInfo::exists(endpoint));
    QProcess child;
    child.start(app.applicationFilePath(),{
        "--child"
    });
    check(child.waitForStarted(1000));
    check(child.waitForReadyRead(2000));
    const auto childEndpoint=QString::fromUtf8(child.readAllStandardOutput()).trimmed();
    check(!childEndpoint.isEmpty());
    child.kill();
    check(child.waitForFinished(2000));
    QLocalSocket lost;
    lost.connectToServer(childEndpoint);
    check(!lost.waitForConnected(500));
    QDir(QFileInfo(childEndpoint).absolutePath()).removeRecursively();
    return 0;
}
