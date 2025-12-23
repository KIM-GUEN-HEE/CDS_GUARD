#include "SendMode.h"
#include "Utils.hpp"
#include "GuardL2.hpp"

#include <iostream>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <cstring>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "CdsGuardServer.hpp"
#include "asio.hpp"
#include "Utils.hpp"

void run_send_mode(const std::string &interface_name, const std::string &dst_mac_str)
{
    
    std::array<uint8_t, 6> src_mac = get_mac_address(interface_name);
    std::array<uint8_t, 6> dst_mac = mac_str_to_bytes(dst_mac_str);
    
    asio::io_context ctx;
    asio::ip::tcp::acceptor acceptor(ctx, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    asio::ip::port_type recv_port = acceptor.local_endpoint().port();
    
    std::jthread discorvery_thread(CdsGuardStartDiscoveryResponder(ctx, recv_port));
    
    std::cout << "[*] SEND MODE: Listening on TCP:" << recv_port << " for encrypted L2 payloads...\n";
    
    while (true)
    {
        asio::ip::tcp::socket recv_sock(ctx);  // 새로 생성된 소켓
        acceptor.accept(recv_sock);            // 클라이언트 연결 수락
        constexpr size_t recv_data_block_size = std::numeric_limits<unsigned short>::max();
        std::vector<uint8_t> recv_data(recv_data_block_size);
        asio::error_code ec;
        size_t readed_byte_size = 0;

        while (true)
        {
            asio::mutable_buffer tcp_sock_buffer(recv_data.data() + readed_byte_size, recv_data.size() - readed_byte_size);
            readed_byte_size += recv_sock.read_some(tcp_sock_buffer, ec);

            if (ec)
            {
                if (asio::error::eof == ec)
                {
                    std::cout << "[SendMode] Connection closed by peer.\n";
                    break;
                }
                else
                {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                    break;
                }
            }
            else if (readed_byte_size == recv_data.size())
            {
                recv_data.resize(recv_data.size() * 2);
            }
        }
        recv_data.resize(readed_byte_size);

        if (!recv_data.empty()) 
        {
            std::cout << "[*] Received " << recv_data.size() << " bytes via TCP. Preparing to send via L2...\n";

            GuardL2Sender l2_sender(interface_name, src_mac, dst_mac);
            if (l2_sender.send_reliable_data(recv_data)) 
            {
                std::cout << "[*] L2 transmission successful.\n";
            } 
            else 
            {
                std::cerr << "[*] L2 transmission failed.\n";
            }
        }
    }
}