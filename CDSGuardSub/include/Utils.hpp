#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <cstring>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <system_error>

inline std::array<uint8_t, 6> get_mac_address(std::string_view _interface_name)
{
    std::array<uint8_t, 6> result = {0};

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) 
    {
        perror("socket");
        return result;  // 빈 MAC 리턴
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, _interface_name.data(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) 
    {
        perror("ioctl");
        close(fd);
        return result;
    }

    close(fd);

    std::memcpy(result.data(), ifr.ifr_hwaddr.sa_data, 6);
    return result;
}

inline std::array<uint8_t, 6> mac_str_to_bytes(const std::string &mac_str) {
    std::array<uint8_t, 6> mac_bytes{};
    std::istringstream iss(mac_str);
    std::string byte_str;
    int i = 0;

    while (std::getline(iss, byte_str, ':') && i < 6) {
        try {
            mac_bytes[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        } catch (...) {
            throw std::invalid_argument("Invalid MAC address format");
        }
        ++i;
    }

    if (i != 6 || iss.rdbuf()->in_avail() != 0) {
        throw std::invalid_argument("Invalid MAC address format");
    }

    return mac_bytes;
}