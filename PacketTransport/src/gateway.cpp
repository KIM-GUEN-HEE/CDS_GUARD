#include "../include/L2SocketTransport.h"
#include "../include/L3SocketTransport.h"

int main() {
    L3Sender sender(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT);
    
    if (!sender.connectToCDS()) return 1;
    std::string msg;
    std::cout<<"메시지 입력:";
    std::cin >> msg;
    if (!sender.sendMessage(msg)) return 1;

    return 0;
}