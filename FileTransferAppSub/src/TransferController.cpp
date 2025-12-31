#include "TransferController.hpp"
#include <QDateTime>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <fstream>
#include <filesystem>
#include <QTcpSocket>
#include <QtEndian>
#include "ProxyProtocol.hpp"

TransferController::TransferController(QObject* p) : QObject(p)
{
    const auto cds_gateway_connected_lambda = [this](const QString& gip, quint16 gport)
    {
        emit logMessage(tr("[DISC] CDS Gateway %1:%2 found").arg(gip).arg(gport));

        cds_gateway_ip_ = gip;
        cds_gateway_port_ = gport;
    };

    connect(&finder_, &CdsGatewayFinder::cds_gateway_found, this,  cds_gateway_connected_lambda);
    connect(&finder_, &CdsGatewayFinder::timeout, this, [&](){  });

    // 프로그램 시작 직후 자동 탐색
    QTimer::singleShot(0, this, [this]{ finder_.discover(1000, true); });

    // 기존 소켓/서버 연결(readyRead 등) 그대로 유지 
    connect(&server_,  &QTcpServer::newConnection, this,&TransferController::onNewConnection);
}

void TransferController::sendData(const QString& ip, quint16 port, const QString& payloadOrPath, bool isText)
{

    // 1차 접속: 제어 포트 (ex. 10090)
    QTcpSocket controlSock;
    controlSock.connectToHost(cds_gateway_ip_, cds_gateway_port_);
    if (!controlSock.waitForConnected(2000)) {
        emit logMessage("Failed to connect to control port"); return;
    }

    // 서버가 보낸 동적 포트 수신
    quint16 cds_gateway_dynamic_port = 0;
    if (!controlSock.waitForReadyRead(2000) ||
        controlSock.read(reinterpret_cast<char*>(&cds_gateway_dynamic_port), 2) != 2) {
        emit logMessage("Failed to receive dynamic port"); return;
    }
    controlSock.disconnectFromHost();
    emit logMessage(tr("Received dynamic port: %1").arg(cds_gateway_dynamic_port));

    // 데이터 전송: 동적 포트에 새로 연결 후 전송
    if (isText)
        sendText(ip, port, cds_gateway_dynamic_port, payloadOrPath);   // 수정 필요
    else
        sendFile(ip, port, cds_gateway_dynamic_port, payloadOrPath);   // 수정 필요

    emit logMessage(tr("sendData() → %1:%2 (%3)").arg(ip).arg(port).arg(isText ? "Text" : "File"));
}

void TransferController::sendText(const QString& dest_ip, quint16 dest_port, quint16 cds_gateway_dynamic_port, const QString& txt)
{
    if (txt.isEmpty()) {
        emit logMessage("No text to send");
        return;
    }

    QTcpSocket sock;
    sock.connectToHost(cds_gateway_ip_, cds_gateway_dynamic_port);
    if (!sock.waitForConnected(2000)) {
        emit logMessage("Failed to connect for text transfer");
        return;
    }

    // 프록시 헤더 전송
    QByteArray proxyHeader = make_proxy_header_to_q_byte_arr(sock.localAddress(), sock.localPort(), QHostAddress(dest_ip), dest_port);
    sock.write(proxyHeader);
    sock.flush();

    QByteArray header;
    header.append("TEXT\0", 5);
    quint64 text_len = static_cast<quint64>(txt.size() + 1); // +1 for null terminator
    header.append(reinterpret_cast<const char*>(&text_len), 8); // 8 bytes for length
    sock.write(header);

    // 텍스트 전송
    QByteArray d = txt.toUtf8();
    if (!d.endsWith('\0')) 
    {
        d.append('\0'); // Ensure null termination
    }
    sock.write(d);
    sock.flush();

    emit logMessage(tr("Sent %1 bytes of text").arg(d.size()));
    sock.waitForBytesWritten();
    sock.waitForDisconnected(8000);
    sock.disconnectFromHost();
}

void TransferController::sendFile(const QString& dest_ip, quint16 dest_port, quint16 cds_gateway_dynamic_port, const QString& guiPath)
{
    if (guiPath.trimmed().isEmpty()) {
        emit logMessage("No file selected");
        return;
    }

    QString localPath = QUrl(guiPath).toLocalFile();
    std::ifstream ifs(std::filesystem::path(localPath.toStdString()), std::ios::binary | std::ios::ate);
    if (!ifs) {
        emit logMessage(tr("File open failed: %1").arg(localPath));
        return;
    }

    QTcpSocket sock;
    sock.connectToHost(cds_gateway_ip_, cds_gateway_dynamic_port);
    if (!sock.waitForConnected(2000)) {
        emit logMessage("Failed to connect for file transfer");
        return;
    }

    // ── 1. ProxyHeader 전송 ─────────────────────────────────────
    QByteArray proxyHeader = make_proxy_header_to_q_byte_arr(sock.localAddress(), sock.localPort(), QHostAddress(dest_ip), dest_port);
    sock.write(proxyHeader);
    sock.flush();

    // ── 2. 파일 메타정보 전송 ──────────────────────────────────
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    const std::size_t CHUNK = 64 * 1024;
    std::vector<char> buf(CHUNK);
    std::size_t sent = 0;

    QByteArray nameUtf8 = QFileInfo(localPath).fileName().toUtf8();
    quint32 nameLen  = nameUtf8.size();
    quint64 file_size = static_cast<quint64>(size);

    QByteArray header;
    header.append("FILE\0", 5);
    header.append(reinterpret_cast<char*>(&nameLen), 4); // 파일 이름 길이
    header.append(nameUtf8); // 파일 이름
    header.append(reinterpret_cast<char*>(&file_size), 8); // 파일 크기 (8 bytes)

    sock.write(header);

    // ── 3. 파일 본문 전송 ──────────────────────────────────────
    while (ifs) {
        ifs.read(buf.data(), CHUNK);
        std::streamsize n = ifs.gcount();
        if (n > 0) {
            sock.write(buf.data(), static_cast<qint64>(n));
            sent += n;
        }
    }

    sock.flush();
    sock.waitForBytesWritten();
    sock.waitForDisconnected();
    sock.disconnectFromHost();

    emit logMessage(tr("Sent file \"%1\" (%2 bytes)").arg(localPath).arg(sent));
}

void TransferController::startListening(quint16 port)
{
    if (server_.listen(QHostAddress::Any, port)) {
        emit logMessage(tr("Listening on %1").arg(port));
        emit listeningChanged();
    }
}

void TransferController::stopListening()
{
    server_.close();
    qDeleteAll(clients_); clients_.clear();
    emit logMessage(tr("Stopped listening"));
    emit listeningChanged();
}

void TransferController::onNewConnection()
{
    qDebug() << "New connection received";
    while (QTcpSocket* connection = server_.nextPendingConnection()) 
    {
        clients_ << connection;
        connect(connection, &QTcpSocket::readyRead, this, &TransferController::onSocketRead);
        connect(connection, &QTcpSocket::errorOccurred, this, &TransferController::onSocketError);
        connect(connection, &QTcpSocket::disconnected, this, &TransferController::onSocketDisconnected);
        connect(connection, &QTcpSocket::disconnected, connection, &QTcpSocket::deleteLater);
    }
}

void TransferController::onSocketRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    received_data_map_[sock].append(sock->readAll());
}

void TransferController::onSocketDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock || !received_data_map_.contains(sock))
    {
        emit logMessage("Socket disconnected (unknown source)");
        return;
    }
    
    qDebug() << "Socket disconnected";
    emit logMessage(tr("Socket disconnected from %1:%2").arg(sock->peerAddress().toString()).arg(sock->peerPort()));
    QByteArray received_data = std::move(received_data_map_[sock]);
    received_data_map_.remove(sock);

    if (received_data.startsWith("FILE\0")) 
    {
        handleFileReceive(received_data);
    }
    else if (received_data.startsWith("TEXT\0")) 
    {
        handleTextReceive(received_data);
    }
    else
    {
        emit logMessage(tr("Unknown data format received from %1:%2").arg(sock->peerAddress().toString()).arg(sock->peerPort()));
    }
}

void TransferController::handleFileReceive(const QByteArray& _received_data)
{
    // 파일 이름 길이
    if (_received_data.size() < 5 + 4)
    {
        emit logMessage("Invalid file header received: name length");
        return;
    }

    quint32 nameLen = qFromLittleEndian<quint32>((const uchar*)_received_data.data() + 5);

    // 파일 이름 + 파일 크기 정보까지 충분한가?
    if (_received_data.size() < 5 + 4 + nameLen + 8)
    {
        emit logMessage("Invalid file header received: file name or file size info");
        return;
    }

    QString file_name = QString::fromUtf8(_received_data.mid(5 + 4, nameLen));
    quint64 file_size = qFromLittleEndian<quint64>((const uchar*)_received_data.data() + 5 + 4 + nameLen);
    quint64 header_size = 5 + 4 + nameLen + 8; // "FILE\0" + name length + file name + file size

    emit logMessage(tr("Receiving file: %1 (%2 bytes)").arg(file_name).arg(file_size));

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QFile file("recv_" + timestamp + "_" + file_name);
    if (!file.open(QIODevice::WriteOnly)) 
    {
        emit logMessage("File open error (write)");
        return;
    }

    quint64 payload_offset = header_size;
    quint64 payload_length = _received_data.size() - payload_offset;

    if (payload_length < file_size) 
    {
        emit logMessage("Error: Incomplete file data");
        return;
    }

    QByteArray file_data = _received_data.mid(payload_offset, file_size);
    qint64 written = file.write(file_data);
    file.close();

    if (written != (qint64)file_size) 
    {
        emit logMessage("File write incomplete or failed");
        return;
    }
    else
    {
        emit logMessage(tr("File %1 saved successfully (%2 bytes)").arg(file.fileName()).arg(written));
    }
}

void TransferController::handleTextReceive(const QByteArray& _received_data)
{
    const int headerMagicSize = 5;
    const int lengthFieldSize = 8;

    if (_received_data.size() < headerMagicSize + lengthFieldSize) {
        emit logMessage("Invalid text header: insufficient data");
        return;
    }

    quint64 textLength = qFromLittleEndian<quint64>(
        reinterpret_cast<const uchar*>(_received_data.data() + headerMagicSize)
    );

    quint64 totalSize = headerMagicSize + lengthFieldSize + textLength;

    if (_received_data.size() < static_cast<int>(totalSize)) {
        emit logMessage("Incomplete text data");
        return;
    }

    QByteArray textData = _received_data.mid(headerMagicSize + lengthFieldSize, textLength);

    // Null terminator 제거 (있다면)
    if (textData.endsWith('\0')) {
        textData.chop(1);  // Remove the null terminator
    }

    QString text = QString::fromUtf8(textData);

    emit logMessage("Text message received:");
    emit logMessage(text);
}

void TransferController::onSocketError(QAbstractSocket::SocketError)
{
    QAbstractSocket* sock = qobject_cast<QAbstractSocket*>(sender());
    if (!sock) {
        emit logMessage("Socket error (unknown source)");
        return;
    }

    emit logMessage(tr("Socket error from %1:%2 → %3")
        .arg(sock->peerAddress().toString())
        .arg(sock->peerPort())
        .arg(sock->errorString()));
}


QByteArray TransferController::make_proxy_header_to_q_byte_arr(const QString& scr_host_ip_qstr, quint16 scr_host_port, const QString& dst_host_ip_qstr, quint16 dst_host_port)
{
    auto raw = make_proxy_header(scr_host_ip_qstr.toStdString(), scr_host_port, dst_host_ip_qstr.toStdString(), dst_host_port);
    return QByteArray(reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()));
}

QByteArray TransferController::make_proxy_header_to_q_byte_arr(const QHostAddress scr_ip_qt_addr, const quint16 src_port, const QHostAddress dst_ip_qt_addr, const quint16 dest_port)
{
    std::array<uint8_t, 16> scr_ip_bytes;
    uint8_t scr_ip_len;
    std::array<uint8_t, 16> dst_ip_bytes;
    uint8_t dst_ip_len;

    if (scr_ip_qt_addr.protocol() == QHostAddress::IPv4Protocol)
    {
        quint32 ipv4 = scr_ip_qt_addr.toIPv4Address();
        scr_ip_bytes[0] = (ipv4 >> 24) & 0xFF;
        scr_ip_bytes[1] = (ipv4 >> 16) & 0xFF;
        scr_ip_bytes[2] = (ipv4 >> 8) & 0xFF;
        scr_ip_bytes[3] = ipv4 & 0xFF;
        scr_ip_len = 4;
    }
    else if (scr_ip_qt_addr.protocol() == QHostAddress::IPv6Protocol)
    {
        QIPv6Address ipv6 = scr_ip_qt_addr.toIPv6Address();
        std::copy(&ipv6.c[0], &ipv6.c[16], scr_ip_bytes.begin());
        scr_ip_len = 16;
    }
    else
    {
        return {};
    }

    if (dst_ip_qt_addr.protocol() == QHostAddress::IPv4Protocol)
    {
        quint32 ipv4 = dst_ip_qt_addr.toIPv4Address();
        dst_ip_bytes[0] = (ipv4 >> 24) & 0xFF;
        dst_ip_bytes[1] = (ipv4 >> 16) & 0xFF;
        dst_ip_bytes[2] = (ipv4 >> 8) & 0xFF;
        dst_ip_bytes[3] = ipv4 & 0xFF;
        dst_ip_len = 4;
    }
    else if (dst_ip_qt_addr.protocol() == QHostAddress::IPv6Protocol)
    {
        QIPv6Address ipv6 = dst_ip_qt_addr.toIPv6Address();
        std::copy(&ipv6.c[0], &ipv6.c[16], dst_ip_bytes.begin());
        dst_ip_len = 16;
    }
    else
    {
        return {};
    }

    std::vector<uint8_t> header_vec = make_proxy_header(std::span<const uint8_t>(scr_ip_bytes.data(), scr_ip_len), src_port, std::span<const uint8_t>(dst_ip_bytes.data(), dst_ip_len), dest_port);
    return QByteArray(reinterpret_cast<const char*>(header_vec.data()), static_cast<int>(header_vec.size()));
}