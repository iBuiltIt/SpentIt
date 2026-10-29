#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()<3 || args.size()>5){
        std::cerr
        << "Usage: spentit-diagnostics ENDPOINT status | events [CURSOR] | wait-operation ID [TIMEOUT_MS] | wait-generation ID [TIMEOUT_MS] | wait-ui REVISION [TIMEOUT_MS]\n";
        return 2;
    }
    const auto mode=args[2];
    const bool wait=mode=="wait-operation" || mode=="wait-generation" || mode=="wait-ui";
    if(mode!="status" && mode!="events" && !wait)return 2;
    bool valid=true;
    qulonglong target=0;
    if(wait || args.size()>3){
        if(args.size()<4)return 2;
        target=args[3].toULongLong(&valid);
        if(!valid)return 2;
    }
    int timeout=10000;
    if(args.size()==5){
        timeout=args[4].toInt(&valid);
        if(!wait || !valid || timeout<1 || timeout>60000)return 2;
    }
    QElapsedTimer timer;
    timer.start();
    do {
        QLocalSocket socket;
        socket.connectToServer(args[1],QIODevice::ReadWrite);
        if(!socket.waitForConnected(1000)){
            std::cerr
            << "Disconnected; operation outcome unknown.\n";
            return 3;
        }
        QJsonObject request{
            {
                "method",mode=="events"?"events":"status"
            }
        };
        if(mode=="events")request["after"]=QString::number(target);
        socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+"\n");
        if(!socket.waitForBytesWritten(1000))return 3;
        QByteArray bytes;
        while(!bytes.endsWith('\n')){
            if(!socket.bytesAvailable() && !socket.waitForReadyRead(1000))return 3;
            bytes+=socket.readAll();
            if(bytes.size()>4*1024*1024)return 3;
        }
        const auto object=QJsonDocument::fromJson(bytes).object();
        if(object.isEmpty() || object.contains("error")){
            std::cerr
            << "Reader rejected request.\n";
            return 3;
        }
        bool done=!wait;
        if(mode=="wait-operation")for(auto a:object.value("outcomes").toArray())if(a.toObject().value("id").toVariant().toULongLong()==target && a.toObject().value("result").toInt()!=0)done=true;
        if(mode=="wait-generation"){
            auto ui=object.value("ui").toObject();
            done=ui.value("generation").toVariant().toULongLong()==target && ui.value("interaction_ready").toInt()==1;
        }
        if(mode=="wait-ui") {
            auto ui=object.value("ui").toObject();
            done=ui.value("revision").toVariant().toULongLong()==target && ui.value("interaction_ready").toInt()==1;
        }
        if(done){
            std::cout
            << bytes.toStdString();
            std::cout.flush();
            return std::cout.good()?0:5;
        }
        if(timer.elapsed()>=timeout){
            std::cerr
            << "Observation timed out; operation outcome is not inferred.\n";
            return 4;
        }
        QThread::msleep(100);
    }
    while(true);
}
