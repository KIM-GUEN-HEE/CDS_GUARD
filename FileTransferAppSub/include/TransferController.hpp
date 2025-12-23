#pragma once
#include <QObject>
#include <QTcpServer>
#include <QFile>
#include <QMap>
#include "CdsGatewayFinder.hpp"

class TransferController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ isListening NOTIFY listeningChanged)

public:
    explicit TransferController(QObject* parent = nullptr);

    Q_INVOKABLE void sendText(const QString& destIp, quint16 destPort, quint16 dynamicPort, const QString& txt);;
    Q_INVOKABLE void sendFile(const QString& destIp, quint16 destPort, quint16 dynamicPort, const QString& path);

    Q_INVOKABLE void startListening(quint16 port);
    Q_INVOKABLE void stopListening();
    bool isListening() const { return server_.isListening(); }

    Q_INVOKABLE void sendData(const QString& ip, quint16 port, const QString& payloadOrPath, bool isText);

signals:
    void status(const QString& s);
    void received(const QString& s);
    void listeningChanged();
    void logMessage(const QString& m);

private slots:
    void onSocketRead();
    void onNewConnection();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError);

private:
    QByteArray make_proxy_header_to_q_byte_arr(const QString& scr_host_ip_qstr, quint16 scr_host_port, const QString& dst_host_ip_qstr, quint16 dst_host_port);
    QByteArray make_proxy_header_to_q_byte_arr(const QHostAddress src_ip, const quint16 src_port, const QHostAddress dest_ip, const quint16 dest_port);
    void handleFileReceive(const QByteArray& received_data_);
    void handleTextReceive(const QByteArray& received_data_);
    CdsGatewayFinder finder_{this};
    QString destIp_{"127.0.0.1"};
    quint16 destPort_{5000};
    QString cds_gateway_ip_;
    quint16 cds_gateway_port_;
    QMap<QTcpSocket*, QByteArray> received_data_map_;
    QTcpServer            server_;
    QList<QTcpSocket*>    clients_;
};
