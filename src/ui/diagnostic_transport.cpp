#include "diagnostic_transport.hpp"
#include <spentit/diagnostics.hpp>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDir>
#include <atomic>
DiagnosticTransport::DiagnosticTransport()=default;
DiagnosticTransport::~DiagnosticTransport(){
    if(host_){
        QMetaObject::invokeMethod(host_,[this]{
            delete host_;host_=nullptr;
        },Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }
}
bool DiagnosticTransport::start(){
    host_=new QObject;
    host_->moveToThread(&thread_);
    thread_.start();
    bool ok=false;
    QMetaObject::invokeMethod(host_,[&]{
        // macOS per-user TMPDIR can exceed sockaddr_un's path limit. The
        // randomly created directory remains owner-only under the sticky temp root.
#ifdef Q_OS_UNIX
        const QString runtimeRoot=QStringLiteral("/tmp");
#else
        const QString runtimeRoot=QDir::tempPath();
#endif
        auto dir=std::make_shared<QTemporaryDir>(runtimeRoot+"/spentit-observe-XXXXXX"); if(!dir->isValid() || !QFile::setPermissions(dir->path(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return; auto* server=new QLocalServer(host_);server->setSocketOptions(QLocalServer::UserAccessOption);server->setMaxPendingConnections(8); endpoint_=dir->path()+"/reader";if(!server->listen(endpoint_))return; auto clients=std::make_shared<int>(0); QObject::connect(server,&QLocalServer::newConnection,host_,[server,dir,clients]{
            while(auto* socket=server->nextPendingConnection()){
                if(*clients>=8){
                    socket->abort();socket->deleteLater();continue;
                }
                ++*clients; socket->setReadBufferSize(1025); QObject::connect(socket,&QObject::destroyed,server,[clients]{
                    --*clients;
                }); auto buffer=std::make_shared<QByteArray>();auto answered=std::make_shared<bool>(false); auto* deadline=new QTimer(socket);deadline->setSingleShot(true);deadline->start(2000); QObject::connect(deadline,&QTimer::timeout,socket,[socket]{
                    socket->abort();socket->deleteLater();
                }); QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater); QObject::connect(socket,&QLocalSocket::readyRead,socket,[socket,buffer,answered]{
                    if(*answered)return;buffer->append(socket->readAll()); auto reject=[&]{
                        *answered=true;socket->write("{\"error\":\"invalid_request\"}\n");socket->disconnectFromServer();
                    }; if(buffer->size()>1024){
                        reject();return;
                    }
                    if(!buffer->contains('\n'))return; if(buffer->count('\n')!=1 || !buffer->endsWith('\n')){
                        reject();return;
                    }
                    QJsonParseError error;const auto doc=QJsonDocument::fromJson(*buffer,&error);const auto obj=doc.object(); const auto method=obj.value("method").toString(); if(error.error!=QJsonParseError::NoError || !doc.isObject() || (method!="status" && method!="events") || obj.size()>(method=="events"?2:1)){
                        reject();return;
                    }
                    for(auto it=obj.begin();it!=obj.end();++it)if(it.key()!="method" && !(method=="events" && it.key()=="after")){
                        reject();return;
                    }
                    bool valid=true;qulonglong after=0;if(obj.contains("after")){
                        if(!obj.value("after").isString()){
                            reject();return;
                        }
                        after=obj.value("after").toString().toULongLong(&valid);
                    }
                    if(!valid){
                        reject();return;
                    }
                    *answered=true; const auto response=spentit::diagnostics::Collector::instance().json(method=="events"?after:UINT64_MAX); socket->write(response.data(),static_cast<qint64>(response.size()));socket->write("\n");socket->disconnectFromServer();
                });
            }
        });ok=true;
    },Qt::BlockingQueuedConnection);
    return ok;
}
