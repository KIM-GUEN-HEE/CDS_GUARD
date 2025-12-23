#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

struct EthHdr
{
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
} __attribute__((packed));

struct IpHdr
{
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

struct UdpHdr
{
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed));

/**
 * @brief IPv4 헤더 체크섬 계산 (RFC 791)
 */
uint16_t compute_ip_checksum(const void *buffer, size_t length)
{
    auto *buf = static_cast<const uint16_t *>(buffer);
    uint32_t sum = 0;
    for (size_t i = 0; i < length / 2; i++)
    {
        sum += ntohs(buf[i]);
    }
    if (length % 2)
    {
        sum += (static_cast<const uint8_t *>(buffer))[length - 1] << 8;
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return htons(static_cast<uint16_t>(~sum));
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <UDP_send_port>\n";
        return 1;
    }
    int udp_port = std::atoi(argv[1]);

    const char *payload_str = "Hello, SEND!, test, test";
    size_t payload_len = std::strlen(payload_str);

    EthHdr eth;

    eth.dst_mac[0] = 0x02;
    eth.dst_mac[1] = 0x00;
    eth.dst_mac[2] = 0x00;
    eth.dst_mac[3] = 0x00;
    eth.dst_mac[4] = 0x00;
    eth.dst_mac[5] = 0x02;

    eth.src_mac[0] = 0x02;
    eth.src_mac[1] = 0x00;
    eth.src_mac[2] = 0x00;
    eth.src_mac[3] = 0x00;
    eth.src_mac[4] = 0x00;
    eth.src_mac[5] = 0x01;
    eth.ethertype = htons(0x0800);

    IpHdr ip;
    ip.version = 4;
    ip.ihl = 5;
    ip.tos = 0;
    ip.tot_len = htons(sizeof(IpHdr) + sizeof(UdpHdr) + payload_len);
    ip.id = htons(0x4321);
    ip.frag_off = htons(0);
    ip.ttl = 64;
    ip.protocol = IPPROTO_UDP;
    ip.checksum = 0;
    inet_pton(AF_INET, "192.168.1.10", &ip.saddr);
    inet_pton(AF_INET, "192.168.2.10", &ip.daddr);
    ip.checksum = compute_ip_checksum(&ip, sizeof(IpHdr));

    UdpHdr udp;
    udp.source = htons(40000);
    udp.dest = htons(40000);
    udp.len = htons(sizeof(UdpHdr) + payload_len);
    udp.checksum = 0;

    std::vector<uint8_t> raw_frame;
    raw_frame.resize(sizeof(EthHdr) + sizeof(IpHdr) + sizeof(UdpHdr) + payload_len);
    size_t offset = 0;

    std::memcpy(raw_frame.data() + offset, &eth, sizeof(EthHdr));
    offset += sizeof(EthHdr);

    std::memcpy(raw_frame.data() + offset, &ip, sizeof(IpHdr));
    offset += sizeof(IpHdr);

    std::memcpy(raw_frame.data() + offset, &udp, sizeof(UdpHdr));
    offset += sizeof(UdpHdr);

    std::memcpy(raw_frame.data() + offset, payload_str, payload_len);
    offset += payload_len;

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
    {
        perror("socket(AF_INET) failed");
        return 1;
    }
    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(udp_port);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    ssize_t sent = sendto(
        sock_fd,
        raw_frame.data(),
        raw_frame.size(),
        0,
        reinterpret_cast<struct sockaddr *>(&dest_addr),
        sizeof(dest_addr));
    if (sent < 0)
    {
        perror("sendto() failed");
        close(sock_fd);
        return 1;
    }
    std::cout << "[>] Sent " << sent << " bytes of raw frame to 127.0.0.1:" << udp_port << "\n";

    close(sock_fd);
    return 0;
}
