#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cstdio>
#include <string.h>


#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 9000


class L3Sender {
private:
    std::string serverIp;
    int serverPort;
    int sockfd;

public:
    L3Sender(const std::string& ip, int port) 
        : serverIp(ip), serverPort(port), sockfd(-1) {}

    ~L3Sender() {
        if (sockfd != -1) close(sockfd);
    }

    //MAC주소 형식 검사
    bool parse_mac_address(const char* mac_str, uint8_t* mac) {
        int values[6];
        if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
                   &values[0], &values[1], &values[2],
                   &values[3], &values[4], &values[5]) != 6) {
            return false;
        }
        for (int i = 0; i < 6; ++i) {
            if (values[i] < 0 || values[i] > 0xFF)
                return false;
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }

    bool connectToCDS() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Socket creation failed.\n";
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);

        if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cerr << "Invalid IP address.\n";
            close(sockfd);
            sockfd = -1;
            return false;
        }

        std::cout << "[*] Connecting to " << serverIp << ":" << serverPort << "...\n";
        if (connect(sockfd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection failed.\n";
            close(sockfd);
            sockfd = -1;
            return false;
        }

        std::cout << "[+] Connected to server.\n";
        return true;
    }

    bool sendMessage(const std::string& message) {
        if (sockfd == -1) {
            std::cerr << "Not connected to server.\n";
            return false;
        }

        ssize_t bytesSent = send(sockfd, message.c_str(), message.length(), 0);
        if (bytesSent < 0) {
            std::cerr << "Send failed.\n";
            return false;
        }

        std::cout << "[+] Sent " << bytesSent << " bytes: " << message << "\n";
        return true;
    }
};

class L3Receiver {
private:
    int port;
    int tcpSock;

public:
    L3Receiver(int p) : port(p), tcpSock(-1) {
        tcpSock = socket(AF_INET, SOCK_STREAM, 0);
        if (tcpSock < 0) {
            perror("socket");
            exit(1);
        }

        // socket option 설정
        int opt = 1;
        if (setsockopt(tcpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt");
            exit(1);
        }
        
        /*
        struct sockaddr_in {
            sa_family_t    sin_family; // 주소 체계 (AF_INET)
            in_port_t      sin_port;   // 포트 번호 (네트워크 바이트 순서)
            struct in_addr sin_addr;   // IP 주소
            char           sin_zero[8]; // padding (사용 안함)
        };

        */

        sockaddr_in serverAddr{}; // 모든 feild 0으로 초기화
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(tcpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("bind");
            close(tcpSock);
            exit(1);
        }

        if (listen(tcpSock, 5) < 0) {
            perror("listen");
            close(tcpSock);
            exit(1);
        }

        std::cout << "[*] TCP Receiver listening on port " << port << "...\n";
    }

    ~L3Receiver() {
        if (tcpSock >= 0) close(tcpSock);
    }

    int receive(char* buffer, size_t bufferSize) {
        int clientSock = accept(tcpSock, nullptr, nullptr);
        if (clientSock < 0) {
            perror("accept");
            return -1;
        }

        std::cout << "[+] Client connected.\n";
        int received = recv(clientSock, buffer, bufferSize, 0);
        std::cout << "[+] Received " << received << " bytes: " << std::string(buffer, received) << "\n";
        close(clientSock);
        return received;
    }
};