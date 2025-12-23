#include "../include/L2SocketTransport.h"
#include "../include/L3SocketTransport.h"




int main() {
    uint8_t SRC_MAC[6];
    std::string INTERFACE_NAME;
    char src_mac_str[18];

    std::cout << "INTERFACE NAME: ";
    std::cin >> INTERFACE_NAME;
    std::cout << "출발지 MAC 주소 입력 (예: 00:15:5d:8e:c1:a4): ";
    std::cin >> src_mac_str;

    uint8_t dummy_mac[6] = {0,0,0,0,0,0};
    L2Sender temp("", dummy_mac);       // 임시 객체, 파싱 함수 쓰기 위함
    
    if (!temp.parse_mac_address(src_mac_str, SRC_MAC)) {
        std::cerr << "출발지 MAC 주소 형식이 잘못되었습니다.\n";
        return 1;
    }

    L2Sender sender(INTERFACE_NAME, SRC_MAC); // 파싱된 MAC 주소를 기반으로 sender 생성
    L3Receiver receiver(DEFAULT_SERVER_PORT);

    while (true) {
        char buffer[BUFFER_SIZE]{};
        int received = receiver.receive(buffer, BUFFER_SIZE);
        if (received <= 0) continue;
        
        char mac_input[18];
        std::cout << "\ndestination MAC 주소를 입력하세요 (예: 00:00:00:00:00:00) [exit 입력 시 종료]: ";
        std::cin >> mac_input;
        
        if (strcmp(mac_input, "exit") == 0 || strcmp(mac_input, "quit") == 0) {
            std::cout << "[*] 프로그램을 종료합니다.\n";
            break;
        }

        uint8_t dest_mac[6];
        if (!sender.parse_mac_address(mac_input, dest_mac)) {
            std::cerr << "잘못된 MAC 주소 형식입니다.\n";
            continue;
        }
        
        if (sender.send(dest_mac, buffer, received)) {
            std::cout << "[+] Packet sent via raw socket to " << mac_input << ".\n";
        } else {
            std::cerr << "[!] Failed to send packet.\n";
        }
    }
    
    return 0;
}
