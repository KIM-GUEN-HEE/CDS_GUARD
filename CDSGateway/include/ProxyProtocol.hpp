#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <span>
#include <asio/ip/address.hpp>

struct ProxyHeader
{
    static constexpr std::uint32_t kMagic = 0x50524732;

    std::array<std::uint8_t, 16> src_ip{};
    std::uint16_t src_port{};
    std::uint8_t src_ip_ver{};
    std::array<std::uint8_t, 16> dest_ip{};
    std::uint16_t dest_port{};
    std::uint8_t dest_ip_ver{};

    inline std::string ipString() const 
    {
        try 
        {
            asio::ip::address addr = asio::ip::make_address_v4(asio::ip::address_v4::bytes_type{dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3]});

            if (dest_ip_ver == 6) 
            {
                asio::ip::address_v6::bytes_type bytes{};
                std::copy(dest_ip.begin(), dest_ip.begin() + 16, bytes.begin());
                addr = asio::ip::make_address_v6(bytes);
            }

            return addr.to_string();
        } 
        catch (const std::exception& e) 
        {
            return "Invalid IP";
        }
    }
};

inline std::vector<std::uint8_t> make_proxy_header(const std::string& scr_ip_str, std::uint16_t src_port, const std::string& dst_ip_str, std::uint16_t dest_port)
{
    ProxyHeader hdr{};
    std::vector<std::uint8_t> result;

    // IP 주소 파싱
    asio::ip::address dest_ip_addr = asio::ip::make_address(dst_ip_str);
    if (dest_ip_addr.is_v4()) 
    {
        hdr.dest_ip_ver = 4;
        auto ipv4_bytes = dest_ip_addr.to_v4().to_bytes(); // std::array<uint8_t, 4>
        std::memcpy(hdr.dest_ip.data(), ipv4_bytes.data(), ipv4_bytes.size());
    } 
    else if (dest_ip_addr.is_v6()) 
    {
        hdr.dest_ip_ver = 6;
        auto ipv6_bytes = dest_ip_addr.to_v6().to_bytes(); // std::array<uint8_t, 16>
        std::memcpy(hdr.dest_ip.data(), ipv6_bytes.data(), ipv6_bytes.size());
    } 
    else 
    {
        throw std::runtime_error("Invalid Dest IP format");
    }

    asio::ip::address scr_ip_addr = asio::ip::make_address(scr_ip_str);
    if (scr_ip_addr.is_v4()) 
    {
        hdr.src_ip_ver = 4;
        auto ipv4_bytes = scr_ip_addr.to_v4().to_bytes(); // std::array<uint8_t, 4>
        std::memcpy(hdr.src_ip.data(), ipv4_bytes.data(), ipv4_bytes.size());
    } 
    else if (scr_ip_addr.is_v6()) 
    {
        hdr.src_ip_ver = 6;
        auto ipv6_bytes = scr_ip_addr.to_v6().to_bytes(); // std::array<uint8_t, 16>
        std::memcpy(hdr.src_ip.data(), ipv6_bytes.data(), ipv6_bytes.size());
    } 
    else 
    {
        throw std::runtime_error("Invalid source IP format");
    }

    result.reserve(4 + 1 + 16 + 2 + 1 + 16 + 2); // Magic + src_ip_ver + src_ip + src_port + dest_ip_ver + dst_ip + dst_port

    constexpr std::uint32_t magic = ProxyHeader::kMagic;
    result.push_back(magic >> 24);
    result.push_back((magic >> 16) & 0xFF);
    result.push_back((magic >> 8) & 0xFF);
    result.push_back(magic & 0xFF);

    result.push_back(hdr.src_ip_ver);
    result.insert(result.end(), hdr.src_ip.begin(), hdr.src_ip.begin() + 16);
    result.push_back(hdr.src_port >> 8);
    result.push_back(hdr.src_port & 0xFF);
    
    result.push_back(hdr.dest_ip_ver);
    result.insert(result.end(), hdr.dest_ip.begin(), hdr.dest_ip.begin() + 16);
    result.push_back(hdr.dest_port >> 8);
    result.push_back(hdr.dest_port & 0xFF);

    return result;
}

inline std::vector<std::uint8_t> make_proxy_header(std::span<const uint8_t> src_ip, std::uint16_t src_port, std::span<const uint8_t> dest_ip, std::uint16_t dest_port)
{

    ProxyHeader hdr{};

    if (src_ip.size() == 4) 
    {
        hdr.src_ip_ver = 4;
        std::copy(src_ip.begin(), src_ip.end(), hdr.src_ip.begin());
    } 
    else if (src_ip.size() == 16) 
    {
        hdr.src_ip_ver = 6;
        std::copy(src_ip.begin(), src_ip.end(), hdr.src_ip.begin());
    } 
    else 
    {
        return {};
    }

    if (dest_ip.size() == 4) 
    {
        hdr.dest_ip_ver = 4;
        std::copy(dest_ip.begin(), dest_ip.end(), hdr.dest_ip.begin());
    } 
    else if (dest_ip.size() == 16) 
    {
        hdr.dest_ip_ver = 6;
        std::copy(dest_ip.begin(), dest_ip.end(), hdr.dest_ip.begin());
    } 
    else 
    {
        return {};
    }

    std::vector<std::uint8_t> result;
    result.reserve(4 + 1 + 16 + 2 + 1 + 16 + 2); // Magic + src_ip_ver + src_ip + src_port + dest_ip_ver + dest_ip + dest_port

    constexpr std::uint32_t magic = ProxyHeader::kMagic;
    result.push_back(magic >> 24);
    result.push_back((magic >> 16) & 0xFF);
    result.push_back((magic >> 8) & 0xFF);
    result.push_back(magic & 0xFF);

    result.push_back(hdr.src_ip_ver);
    result.insert(result.end(), hdr.src_ip.begin(), hdr.src_ip.begin() + 16);
    result.push_back(hdr.src_port >> 8);
    result.push_back(hdr.src_port & 0xFF);
    
    result.push_back(hdr.dest_ip_ver);
    result.insert(result.end(), hdr.dest_ip.begin(), hdr.dest_ip.begin() + 16);
    result.push_back(hdr.dest_port >> 8);
    result.push_back(hdr.dest_port & 0xFF);

    return result;
}