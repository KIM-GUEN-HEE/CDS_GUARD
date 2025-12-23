#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <iostream>
#include <cstring>
#include <array>
#include <fstream>
#include <asio.hpp>
#include <QApplication>
#include "ProxyProtocol.hpp"
#include "CdsGatewayDiscovery.hpp"
#include "MainWindow.hpp"
#include "FilteringInterface.hpp"
#include "CdsGateway.hpp"
#include "protocol/ProtocolEngine.h"
#include "protocol/ShiftModule.h"
#include "protocol/PaddingModule.h"
#include "protocol/EncryptionModule.h"
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

ProtocolEngine GetProtocolEngine()
{
    ProtocolEngine engine;
    engine.addModule(std::make_unique<ShiftModule>(8));
    engine.addModule(std::make_unique<EncryptionModule>(std::vector<uint8_t>(32, 0x01))); // 예시로 32바이트 키 사용
    engine.addModule(std::make_unique<PaddingModule>(std::vector<uint8_t>{0,0,0,0}));

    return engine;
}

void DispatchFilteredPayloadTcp(ReceivedPayload&& payload)
{   
    try 
    {
        asio::io_context io;
        std::error_code ec;
        asio::ip::tcp::socket socket(io);
        asio::ip::tcp::endpoint guard_endpoint = GetCurrentGuardEndpoint();
        const static ProtocolEngine protocol_engine = GetProtocolEngine();
        socket.connect(guard_endpoint, ec);

        while (ec)
        {
            CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "Connection failed to", guard_endpoint.address().to_string(), ":", ec.message(), ". Retrying...\n");
            
            InvalidateGuardEndpoint(guard_endpoint);
            
            guard_endpoint = GetCurrentGuardEndpoint();
            
            socket.close();
            socket.open(guard_endpoint.protocol());
            socket.connect(guard_endpoint, ec);
        }

        constexpr size_t kOriginalDestInfoSize = 1 + 1 + 16 + 2; // IP 버전(1) + IP 길이(1) + IP 주소(16) + 포트(2)
        std::vector<uint8_t> buffer;
        buffer.reserve(kOriginalDestInfoSize);
        
        // --- [1] 헤더에 원래 출발지/목적지 정보 삽입 ---
        buffer.push_back(static_cast<uint8_t>(payload.src_ip_ver));
        buffer.insert(buffer.end(), payload.src_ip.begin(), payload.src_ip.begin() + payload.src_ip_len);
        buffer.push_back(static_cast<uint8_t>((payload.src_port >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(payload.src_port & 0xFF));
        
        buffer.push_back(static_cast<uint8_t>(payload.dest_ip_ver));
        buffer.insert(buffer.end(), payload.dest_ip.begin(), payload.dest_ip.begin() + payload.dest_ip_len);
        buffer.push_back(static_cast<uint8_t>((payload.dest_port >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(payload.dest_port & 0xFF));
        
        std::vector<uint8_t> new_payload_vec;
        size_t new_payload_size = 5 + (payload.is_file ? (4 + payload.file_name.size() + 8) : 8) + payload.data.size();
        new_payload_vec.reserve(new_payload_size);

        // --- [2] 실제 데이터 형식 구성 ---
        if (payload.is_file) 
        {
            new_payload_vec.insert(new_payload_vec.end(), {'F', 'I', 'L', 'E', '\0'});

            uint32_t nameLen = static_cast<uint32_t>(payload.file_name.size());
            for (int i = 0; i < 4; ++i)
                new_payload_vec.push_back((nameLen >> (i * 8)) & 0xFF);

            new_payload_vec.insert(new_payload_vec.end(), payload.file_name.begin(), payload.file_name.end());

            uint64_t fileSize = payload.data.size();
            for (int i = 0; i < 8; ++i)
                new_payload_vec.push_back((fileSize >> (i * 8)) & 0xFF);

            new_payload_vec.insert(new_payload_vec.end(), payload.data.begin(), payload.data.end());
        }
        else 
        {
            new_payload_vec.insert(new_payload_vec.end(), {'T', 'E', 'X', 'T', '\0'});

            uint64_t textLen = payload.data.size();
            for (int i = 0; i < 8; ++i) new_payload_vec.push_back((textLen >> (i * 8)) & 0xFF);

            new_payload_vec.insert(new_payload_vec.end(), payload.data.begin(), payload.data.end());
        }

        new_payload_vec = protocol_engine.encrypt(std::span<const uint8_t>(new_payload_vec.begin(), new_payload_vec.end()));
        buffer.insert(buffer.end(), new_payload_vec.begin(), new_payload_vec.end());

        // --- TCP로 데이터 전송 ---
        asio::write(socket, asio::buffer(buffer));
        socket.close();

        std::cout << "Payload sent via TCP\n";
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Error sending TCP payload: " << e.what() << "\n";
    }
}

void DispatchFilteredPayloadUdp(ReceivedPayload&& payload)
{
    try 
    {
        asio::io_context io;

        // 전송할 고정 대상 IP/PORT
        asio::ip::udp::endpoint fixed_endpoint(asio::ip::make_address("127.0.0.1"), 54321);

        asio::ip::udp::socket socket(io);
        socket.open(fixed_endpoint.protocol());

        std::vector<uint8_t> buffer;

        // --- [1] 헤더에 원래 목적지 정보 삽입 ---
        buffer.push_back(static_cast<uint8_t>(payload.dest_ip_ver));
        buffer.push_back(static_cast<uint8_t>(payload.dest_ip_len));

        if (payload.dest_ip_len > 0 && payload.dest_ip.size() >= payload.dest_ip_len)
        {
            buffer.insert(buffer.end(), payload.dest_ip.begin(), payload.dest_ip.begin() + payload.dest_ip_len);
        }
        else 
        {
            CDS_GATEWAY_DEBUG_ERROR_LOG("[ERROR]", "Invalid dest_ip in payload");
            return;
        }

        // 포트는 2바이트 (네트워크 바이트 순서, big-endian)
        buffer.push_back(static_cast<uint8_t>((payload.dest_port >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(payload.dest_port & 0xFF));

        // --- [2] 이어서 실제 데이터 전송 형식 구성 ---
        if (payload.is_file) 
        {
            buffer.insert(buffer.end(), {'F', 'I', 'L', 'E', '\0'});

            uint32_t nameLen = static_cast<uint32_t>(payload.file_name.size());
            for (int i = 0; i < 4; ++i)
                buffer.push_back((nameLen >> (i * 8)) & 0xFF);

            buffer.insert(buffer.end(), payload.file_name.begin(), payload.file_name.end());

            uint64_t fileSize = payload.data.size();
            for (int i = 0; i < 8; ++i)
                buffer.push_back((fileSize >> (i * 8)) & 0xFF);

            buffer.insert(buffer.end(), payload.data.begin(), payload.data.end());
        }
        else 
        {
            buffer.insert(buffer.end(), {'T', 'E', 'X', 'T', '\0'});

            std::string text(reinterpret_cast<const char*>(payload.data.data()), payload.data.size());
            if (text.back() != '\0') text.push_back('\0');

            uint64_t textLen = text.size();
            for (int i = 0; i < 8; ++i)
                buffer.push_back((textLen >> (i * 8)) & 0xFF);

            buffer.insert(buffer.end(), text.begin(), text.end());
        }

        // --- Send the buffer ---
        socket.send_to(asio::buffer(buffer), fixed_endpoint);
        socket.close();

        std::cout << "Payload sent via UDP to fixed destination localhost:54321\n";
    }
    catch (std::exception& e) {
        std::cerr << "Error sending UDP payload: " << e.what() << "\n";
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    CdsGuardMainWindow w;
    w.show();

    asio::io_context ctx;

    // 1. 동적 포트 할당용 acceptor 생성
    asio::ip::tcp::acceptor acceptor(ctx, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    uint16_t control_port = acceptor.local_endpoint().port();

    std::cout << "Listening on control port: " << control_port << "\n";

    // 2. 해당 포트를 discovery responder에 전달 및 accepet_thread 생성
    std::jthread discovery_thread = CdsGatewayStartDiscoveryResponder(ctx, control_port);
    std::jthread accept_thread(CdsGatewayConnectionAcceptLoop, std::ref(ctx), std::ref(acceptor));

    // 3. 필터링 작업 스레드 생성
    std::jthread guard_filtering_thread(CdsGatewayFilteringProcessingLoop, DispatchFilteredPayloadTcp);

    int result = app.exec();
    
    discovery_thread.request_stop();
    accept_thread.request_stop();
    guard_filtering_thread.request_stop();

    CdsGatewayWakeupDiscovery();
    CdsGatewayWakeupConnectionAcceptor(control_port);
    CdsGatewayWakeupFilteringProcessingLoop();

    discovery_thread.join();
    accept_thread.join();
    guard_filtering_thread.join();

    return result;
}