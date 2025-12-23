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

#define BUFFER_SIZE 1024

class L2Sender {
private:
    std::string interfaceName;
    uint8_t srcMac[6];

public:
    L2Sender(const std::string& iface, const uint8_t* src) : interfaceName(iface) {
        memcpy(srcMac, src, 6);
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

    //parsing한 MAC주소로 SEND
    bool send(const uint8_t* destMac, const char* payload, int payloadLen) {
        int rawSock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (rawSock < 0) {
            perror("raw socket");
            return false;
        }

        
        struct ifreq ifr{};
        strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ);
        if (ioctl(rawSock, SIOCGIFINDEX, &ifr) == -1) {
            perror("ioctl");
            close(rawSock);
            return false;
        }

        int ifindex = ifr.ifr_ifindex;

        //Ethernet 계층 전송용 주소 구조체
        sockaddr_ll socket_address{};
        socket_address.sll_ifindex = ifindex;
        socket_address.sll_halen = ETH_ALEN;
        memcpy(socket_address.sll_addr, destMac, 6);

        //DEBUG
        printf("Sending with Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
       srcMac[0], srcMac[1], srcMac[2], srcMac[3], srcMac[4], srcMac[5]);


        //Ethernet 프레임 생성
        uint8_t frame[1514]{};
        memcpy(frame, destMac, 6);                  //목적지 MAC 주소
        memcpy(frame + 6, srcMac, 6);               //출발지 MAC 주소
        frame[12] = 0x88;                           //EtherType 상위 바이트
        frame[13] = 0xb5;                           //EtherType 하위 바이트
        memcpy(frame + 14, payload, payloadLen);    //보낼 데이터
        int frame_len = 14 + payloadLen;

        if (sendto(rawSock, frame, frame_len, 0, (sockaddr*)&socket_address, sizeof(socket_address)) < 0) {
            perror("sendto");
            close(rawSock);
            return false;
        }

        close(rawSock);
        return true;
    }
};

class L2Receiver {
private:
    int rawSock;
    std::string interfaceName;
    uint8_t expectedSrcMac[6];

public:
    L2Receiver(const std::string& iface, const uint8_t expectedMac[6])
        : rawSock(-1), interfaceName(iface) {
        memcpy(expectedSrcMac, expectedMac, 6);
    }

    ~L2Receiver() {
        if (rawSock != -1) close(rawSock);
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

    bool init() {
        rawSock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (rawSock < 0) {
            perror("socket");
            return false;
        }

        struct ifreq ifr{};
        strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ);
        if (ioctl(rawSock, SIOCGIFINDEX, &ifr) == -1) {
            perror("ioctl");
            return false;
        }

        struct sockaddr_ll sll{};
        sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifr.ifr_ifindex;
        sll.sll_protocol = htons(ETH_P_ALL);

        if (bind(rawSock, (struct sockaddr*)&sll, sizeof(sll)) == -1) {
            perror("bind");
            return false;
        }

        std::cout << "[*] Listening on interface: " << interfaceName << "\n";
        return true;
    }


    // L2 raw packet 수신 함수
    bool rawReceiver() {
        uint8_t buffer[BUFFER_SIZE]{};

        ssize_t len = recvfrom(rawSock, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (len < 14) return false;

        uint16_t ethertype = (buffer[12] << 8) | buffer[13];
        if (ethertype != 0x88B5) return false;

        const uint8_t* src_mac = buffer + 6;
        if (memcmp(src_mac, expectedSrcMac, 6) != 0) return false;

        // (선택) 디버깅 정보 출력
        std::cout << "[+] Valid Packet Received! Length: " << len << "\n";
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               src_mac[0], src_mac[1], src_mac[2],
               src_mac[3], src_mac[4], src_mac[5]);

        std::cout << "Payload: " << (char*)(buffer + 14) << "\n";

        return true;
    }
    
};
