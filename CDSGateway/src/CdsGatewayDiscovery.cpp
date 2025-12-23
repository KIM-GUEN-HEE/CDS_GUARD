#include "CdsGatewayDiscovery.hpp"
#include <openssl/hmac.h>
#include <random>
#include <algorithm>
#include <span>

std::array<std::uint8_t,32> hmacPsk(std::span<const std::uint8_t> data)
{
    std::array<std::uint8_t,32> out;
    unsigned int len=0;
    HMAC(EVP_sha256(), kPsk.data(), kPsk.size(), data.data(), data.size(), out.data(), &len);
    return out;
}

std::array<std::uint8_t,32> hmacPsk(const std::vector<std::uint8_t>& data)
{
    std::array<std::uint8_t,32> out;
    unsigned int len=0;
    HMAC(EVP_sha256(), kPsk.data(), kPsk.size(), data.data(), data.size(), out.data(), &len);
    return out;
}

std::array<std::uint8_t,12> randomNonce()
{
    std::random_device rd;
    std::array<std::uint8_t,12> n;

    std::generate(n.begin(), n.end(), [&rd]() { return static_cast<std::uint8_t>(rd()); });

    return n;
}
