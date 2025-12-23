#include "GuardL2.hpp"
#include "Utils.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <asio.hpp>
#include "protocol/ProtocolEngine.h"
#include "protocol/ShiftModule.h"
#include "protocol/PaddingModule.h"
#include "protocol/EncryptionModule.h"

ProtocolEngine GetProtocolEngine()
{
    ProtocolEngine engine;
    engine.addModule(std::make_unique<ShiftModule>(8));
    engine.addModule(std::make_unique<EncryptionModule>(std::vector<uint8_t>(32, 0x01))); // 예시로 32바이트 키 사용
    engine.addModule(std::make_unique<PaddingModule>(std::vector<uint8_t>{0,0,0,0}));

    return engine;
}

void run_recv_mode(const std::string &interface_name)
{
    const static ProtocolEngine protocol_engine = GetProtocolEngine();

    std::cout << "[*] RECV MODE: Listening on L2 for frames on interface " << interface_name << "...\n";

    try
    {
        std::array<uint8_t, 6> self_mac = get_mac_address(interface_name);

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 self_mac[0], self_mac[1], self_mac[2], self_mac[3], self_mac[4], self_mac[5]);
        std::cout << "[*] My MAC address is: " << mac_str << "\n";

        // GuardL2Receiver 객체를 생성. 생성자에서 Raw 소켓 생성 및 바인딩이 이루어짐짐
        GuardL2Receiver l2_receiver(interface_name, self_mac);
        asio::io_context ctx;

        // 3무한 루프를 돌며 계속해서 새로운 데이터 전송을 대기
        while (true)
        {
            // 데이터 수신을 시작합니다.  START -> DATA -> END 프로토콜 전체가 완료될 때까지 블로킹됩니다.
            std::vector<uint8_t> recv_data = l2_receiver.receive_reliable_data();

            // 데이터 수신 성공 여부를 확인
            if (!recv_data.empty())
            {
                std::cout << "\n[RECV-MODE] Successfully received " << recv_data.size() << " bytes of data.\n";

                uint8_t src_ip_ver = recv_data[0];
                uint8_t src_ip_len = src_ip_ver == 4 ? 4 : 16;
                uint8_t dest_ip_ver = recv_data[1 + src_ip_len + 2];
                uint8_t dest_ip_len = dest_ip_ver == 4 ? 4 : 16;

                size_t src_info_size = 1 + src_ip_len + 2; // src_ip_ver + src_ip_len + src_port (2 bytes)
                size_t dest_info_size = 1 + dest_ip_len + 2; // dest
                size_t header_size = src_info_size + dest_info_size;

                std::array<uint8_t, 16> src_ip_bytes_arr;
                std::array<uint8_t, 16> dest_ip_bytes_arr;

                for (size_t i = 0; i < src_ip_len; i++)
                {
                    src_ip_bytes_arr[i] = recv_data[1 + i];
                }
                
                for (size_t i = 0; i < dest_ip_len; i++)
                {
                    dest_ip_bytes_arr[i] = recv_data[src_info_size + 1 + i];
                }
                
                asio::ip::port_type src_port = recv_data[1 + src_ip_len] << 8 | recv_data[1 + src_ip_len + 1];
                asio::ip::port_type dest_port = recv_data[src_info_size + 1 + dest_ip_len] << 8 | recv_data[src_info_size + 1 + dest_ip_len + 1];
                asio::ip::address src_ip;
                asio::ip::address dest_ip;

                if (src_ip_ver == 4)
                {
                    src_ip = asio::ip::address_v4(asio::ip::address_v4::bytes_type(std::array<uint8_t, 4>{src_ip_bytes_arr[0], src_ip_bytes_arr[1], src_ip_bytes_arr[2], src_ip_bytes_arr[3]}));
                }
                else if (src_ip_ver == 6)
                {
                    src_ip = asio::ip::address_v6(asio::ip::address_v6::bytes_type(src_ip_bytes_arr));
                }
                
                if (dest_ip_ver == 4)
                {
                    dest_ip = asio::ip::address_v4(asio::ip::address_v4::bytes_type(std::array<uint8_t, 4>{dest_ip_bytes_arr[0], dest_ip_bytes_arr[1], dest_ip_bytes_arr[2], dest_ip_bytes_arr[3]}));
                }
                else if (dest_ip_ver == 6)
                {
                    dest_ip = asio::ip::address_v6(asio::ip::address_v6::bytes_type(dest_ip_bytes_arr));
                }
                
                asio::ip::tcp::endpoint dest_endpoint(dest_ip, dest_port);
                std::error_code ec;
                
                asio::ip::tcp::socket send_sock(ctx);
                send_sock.open(dest_endpoint.protocol(), ec);
                
                std::vector<uint8_t> decrypted_data = protocol_engine.decrypt(std::span<const uint8_t>{recv_data.begin() + header_size, recv_data.end()});

                if (ec)
                {
                    std::cerr << "[RECV-MODE] Error opening socket: " << ec.message() << "\n";
                    continue; // 소켓 열기에 실패하면 다음 루프로 넘어갑니다.
                }

                send_sock.connect(dest_endpoint, ec);
                if (ec)
                {
                    std::cerr << "[RECV-MODE] Error connecting to " << dest_endpoint << ": " << ec.message() << "\n";
                    send_sock.close();
                    continue;
                }

                asio::write(send_sock, asio::buffer(decrypted_data), ec);

                if (ec)
                {
                    std::cerr << "[RECV-MODE] Error sending data: " << ec.message() << "\n";
                }
                else
                {
                    std::cout << "[RECV-MODE] Data sent successfully to " << dest_ip.to_string() << ":" << dest_port << "\n";
                }
                
                send_sock.close();
                // 수신된 데이터를 고유한 이름의 파일로 저장
                
                // 파일 이름 생성을 위한 타임스탬프
                // auto now = std::chrono::system_clock::now();
                // auto in_time_t = std::chrono::system_clock::to_time_t(now);
                // std::stringstream ss;
                // ss << "received_file_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".bin";
                // std::string filename = ss.str();

                // std::cout << "[RECV-MODE] Saving data to file: " << filename << "\n";
                // std::ofstream output_file(filename, std::ios::binary);
                // if (output_file)
                // {
                //     output_file.write(reinterpret_cast<const char*>(recv_data.data()), recv_data.size());
                //     output_file.close();
                //     std::cout << "[RECV-MODE] File saved successfully.\n";
                // }
                // else
                // {
                //     std::cerr << "[RECV-MODE] Error: Could not open file " << filename << " for writing.\n";
                // }
            }
            else
            {
                // receive_reliable_data 함수 내부에서 전송 실패로 판단한 경우
                std::cerr << "[RECV-MODE] Data transfer failed or resulted in empty data.\n";
            }
            std::cout << "\n[*] Waiting for next transmission...\n";
        }
    }
    catch (const std::runtime_error& e)
    {
        // GuardL2Receiver 생성자 등에서 발생할 수 있는 치명적 오류 처리
        std::cerr << "[RECV-MODE:FATAL] A critical error occurred: " << e.what() << std::endl;
    }
}