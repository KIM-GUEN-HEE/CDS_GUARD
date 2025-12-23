#include "CdsGatewayDiscovery.hpp"
#include <random>
#include <algorithm>

#ifdef USE_OPENSSL_HMAC
#include <openssl/hmac.h>

std::array<std::uint8_t,32> hmacPsk(const std::vector<std::uint8_t>& data)
{
    std::array<std::uint8_t,32> out;
    unsigned int len=0;
    HMAC(EVP_sha256(), kPsk.data(), kPsk.size(), data.data(), data.size(), out.data(), &len);
    return out;
}

#else

#include <QByteArray>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

std::array<std::uint8_t, 32> hmacPsk(const std::vector<std::uint8_t>& data)
{
    const QByteArray key(reinterpret_cast<const char*>(kPsk.data()), kPsk.size());
    const QByteArray message(reinterpret_cast<const char*>(data.data()), data.size());

    QByteArray hmac_result = QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);

    std::array<std::uint8_t, 32> out;
    if (hmac_result.size() == 32) {
        std::copy(hmac_result.constBegin(), hmac_result.constEnd(), out.begin());
    } else {
        out.fill(0); // 오류 처리
    }
    
    return out;
}

#endif

std::array<std::uint8_t,12> randomNonce()
{
    std::random_device rd;
    std::array<std::uint8_t,12> n;

    std::generate(n.begin(), n.end(), [&rd]() { return static_cast<std::uint8_t>(rd()); });

    return n;
}
