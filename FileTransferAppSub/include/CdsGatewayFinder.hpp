#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <array>
#include <vector>
#include "CdsGatewayDiscovery.hpp"

class CdsGatewayFinder : public QObject
{
    Q_OBJECT
public:
    explicit CdsGatewayFinder(QObject* parent = nullptr) : QObject(parent)
    {
        socket_.bind(15051, QUdpSocket::ShareAddress);
        connect(&socket_, &QUdpSocket::readyRead, this, &CdsGatewayFinder::handlePong);

        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, &CdsGatewayFinder::timeout);

        retryTimer_.setInterval(3000);
        connect(&retryTimer_, &QTimer::timeout, this, [this] { discover(timeout_msec_, true);});
    }

    void discover(int msec = 1000, bool retry = false)
    {
        timeout_msec_ = msec;
        retry_enabled_ = retry;

        nonce_ = randomNonce();

        QByteArray pkt;
        pkt.append("PING", 4);
        pkt.append(reinterpret_cast<const char*>(nonce_.data()), 12);

        auto mac = hmacPsk({ pkt.begin(), pkt.end() });
        pkt.append(reinterpret_cast<const char*>(mac.data()), 32);

        socket_.writeDatagram(pkt, QHostAddress::Broadcast, 15050);
        timer_.start(msec);

        if (retry_enabled_ && !retryTimer_.isActive())
        {
            retryTimer_.start();
        }
    }

signals:
    void cds_gateway_found(QString ip, quint16 port);
    void timeout();

private slots:
    void handlePong()
    {
        while (socket_.hasPendingDatagrams())
        {
            QByteArray d; 
            d.resize(socket_.pendingDatagramSize());
            QHostAddress from; quint16 srcPort = 0;
            socket_.readDatagram(d.data(), d.size(), &from, &srcPort);

            if (d.size() != 4 + 2 + 12 + 32) continue;
            if (std::memcmp(d.data(), "PONG", 4) != 0) continue;

            quint16 proxyPort = *reinterpret_cast<const quint16*>(d.data() + 4);
            if (std::memcmp(d.data() + 6, nonce_.data(), 12) != 0) continue;

            std::vector<std::uint8_t> body(d.begin(), d.begin() + 4 + 2 + 12);
            auto macRemote = *reinterpret_cast<const std::array<std::uint8_t, 32>*>(d.data() + 18);
            if (hmacPsk(body) != macRemote) continue;           // 인증 실패

            timer_.stop();
            retryTimer_.stop(); // 성공했으므로 재시도 중지

            QString ip = from.toString();
            if (ip.startsWith("::ffff:")) ip = ip.mid(7);

            emit cds_gateway_found(ip, proxyPort);
        }
    }

    void handleTimeout()
    {
        emit timeout();
    }

private:
    QUdpSocket                         socket_;
    QTimer                             timer_;   ///< discovery 타임아웃
    QTimer     retryTimer_;     // 반복 재시도
    std::array<std::uint8_t, 12>       nonce_;
    bool       retry_enabled_ = false;
    int        timeout_msec_ = 1000;
};
