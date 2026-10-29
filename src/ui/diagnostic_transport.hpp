#pragma once
#include <QThread>
#include <QString>
#include <memory>
// Dedicated event loop: published observations remain readable during GUI stalls.
class DiagnosticTransport {
    public: DiagnosticTransport();
    ~DiagnosticTransport();
    bool start();
    QString endpoint() const {
        return endpoint_;
    }
    private: QThread thread_;
    QObject* host_=nullptr;
    QString endpoint_;
};
