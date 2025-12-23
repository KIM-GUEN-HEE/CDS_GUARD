#include "../include/L2SocketTransport.h"



int main() {
    uint8_t EXPECTED_SRC_MAC[6];
    std::string INTERFACE_NAME;
    char mac_input[18];

    std::cout << "INTERFACE NAME: ";
    std::cin >> INTERFACE_NAME;
    std::cout << "source MAC 주소(expected source address)를 입력하세요 (예: 00:15:5d:8e:c1:a4): ";
    std::cin >> mac_input;

    uint8_t dummy_mac[6]={0,0,0,0,0,0};
    L2Receiver temp("",dummy_mac);
    
    
    if (!temp.parse_mac_address(mac_input,EXPECTED_SRC_MAC)) {
        std::cerr << "잘못된 MAC 주소 형식입니다.\n";
        return 1;
    }

    L2Receiver receiver(INTERFACE_NAME, EXPECTED_SRC_MAC);
    if (!receiver.init()) {
        std::cerr << "[!] Failed to initialize receiver.\n";
        return 1;
    }

    while(true){
        if(!receiver.rawReceiver()) continue;
    }

    return 0;
}