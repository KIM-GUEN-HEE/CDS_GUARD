#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>
#include <thread>
#include <source_location>
#include "asio.hpp"

#if __cplusplus >= 202302L
    #include <print>
    #define CDS_GATEWAY_HAS_STD_PRINT 1
#else
    #define CDS_GATEWAY_HAS_STD_PRINT 0
#endif

#ifdef CDS_GATEWAY_ENABLE_DEBUG_LOG
    static constexpr bool kCdsGatewayEnableDebugLog = true;
#else
    static constexpr bool kCdsGatewayEnableDebugLog = false;
#endif

#define CDS_GATEWAY_DEBUG_LOG(...)       CdsGatewayDebugLog(std::source_location::current(), __VA_ARGS__)
#define CDS_GATEWAY_DEBUG_ERROR_LOG(...) CdsGatewayDebugErrorLog(std::source_location::current(), __VA_ARGS__)

template <typename... Args, typename Loc = std::source_location>
inline void CdsGatewayDebugLog(const std::source_location& loc, Args&&... args)
{
    if constexpr (kCdsGatewayEnableDebugLog) 
    {
        #if CDS_GATEWAY_HAS_STD_PRINT
            std::print("[{}] ", loc.function_name());
            (std::print(std::forward<Args>(args)...));
        #else
            std::cout << "[" << loc.function_name() << "] ";
            const char* sep = "";
            (((std::cout << sep << std::forward<Args>(args)), sep = " "), ...);
        #endif
    }
}

template <typename... Args, typename Loc = std::source_location>
inline void CdsGatewayDebugErrorLog(const std::source_location& loc, Args&&... args)
{
    #if CDS_GATEWAY_HAS_STD_PRINT
        std::print(stderr, "[{}] ", loc.function_name());
        (std::print(stderr, "{} ", std::forward<Args>(args)), ...);
    #else
        std::cout << "[" << loc.function_name() << "] ";
        const char* sep = "";
        (((std::cout << sep << std::forward<Args>(args)), sep = " "), ...);
    #endif
}

struct ReceivedPayload 
{
    bool is_file;
    bool is_txt_file;
    std::string file_name;
    std::vector<uint8_t> data;
    std::uint8_t src_ip_ver;
    std::uint8_t src_ip_len;
    std::array<std::uint8_t, 16> src_ip;
    uint16_t src_port;
    std::uint8_t dest_ip_ver;
    std::uint8_t dest_ip_len;
    std::array<std::uint8_t, 16> dest_ip;
    uint16_t dest_port;
    std::chrono::system_clock::time_point ts;

    std::string get_src_ip_to_string() const;
    std::string get_src_port_to_string() const;
    std::string get_dest_ip_to_string() const;
    std::string get_dest_port_to_string() const;
};

inline constexpr size_t MAX_CONNECTIONS = 4;
inline constexpr size_t MAX_TASKS = 16;
inline constexpr std::chrono::seconds FUTURE_TIMEOUT(30);
inline constexpr uint32_t PROXY_MAGIC = 0x12345678;

void FileSinkSession(asio::ip::tcp::socket sock);
void ControlSession(asio::io_context& ctx, asio::ip::tcp::socket control_sock);
asio::ip::tcp::endpoint GetCurrentGuardEndpoint();
std::optional<asio::ip::tcp::endpoint> CdsGatewayDiscorverGuard(asio::io_context& io);
void InvalidateGuardEndpoint(const asio::ip::tcp::endpoint& failed_endpoint);
std::jthread CdsGatewayStartDiscoveryResponder(asio::io_context& ctx, std::uint16_t controlPort);
void CdsGatewayConnectionAcceptLoop(std::stop_token stoken, asio::io_context& ctx, asio::ip::tcp::acceptor& acceptor);
std::optional<asio::ip::tcp::endpoint> CdsGatewayDiscorverGuard(asio::io_context& ctx);
void CdsGatewayFilteringProcessingLoop(std::stop_token stoken, std::function<void(ReceivedPayload&&)> callback_func);
void CdsGatewayWakeupFilteringProcessingLoop();
void CdsGatewayWakeupDiscovery(uint16_t port = 15050);
void CdsGatewayWakeupConnectionAcceptor(uint16_t port);