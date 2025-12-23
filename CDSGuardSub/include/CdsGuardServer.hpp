#include <thread>
#include <asio.hpp>
#include <bit>
#include <CdsGatewayDiscovery.hpp>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

std::jthread CdsGuardStartDiscoveryResponder(asio::io_context& ctx, std::uint16_t controlPort)
{
    asio::ip::udp::socket sock(ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), 15052));
    sock.set_option(asio::ip::udp::socket::reuse_address(true));

    return std::jthread([sock = std::move(sock), controlPort](std::stop_token stoken) mutable 
    {
        std::array<char, 64> buf;
        asio::ip::udp::endpoint from;

        while (!stoken.stop_requested())
        {
            std::size_t n = sock.receive_from(asio::buffer(buf), from, 0);
            if (n < 4 || std::memcmp(buf.data(), "GUAR", 4) != 0) continue;

            std::vector<std::uint8_t> verify(buf.begin(), buf.begin() + 4 + 12);
            if (hmacPsk(verify) != *reinterpret_cast<std::array<std::uint8_t, 32>*>(buf.data() + 16))
                continue;

            std::vector<std::uint8_t> pong = { 'R','A','U','G' };

            uint16_t network_order_port = htons(controlPort);
            pong.push_back(static_cast<uint8_t>((network_order_port >> 8) & 0xFF));
            pong.push_back(static_cast<uint8_t>(network_order_port & 0xFF));
            
            pong.insert(pong.end(), buf.begin() + 4, buf.begin() + 16);
            auto mac = hmacPsk(pong);
            pong.insert(pong.end(), mac.begin(), mac.end());

            sock.send_to(asio::buffer(pong), asio::ip::udp::endpoint(from.address(), 15053));
        }
    });
}