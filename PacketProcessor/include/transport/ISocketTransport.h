#pragma once

#include <vector>
#include <cstdint>

class ISocketTransport {
public:
    virtual ~ISocketTransport() = default;
    virtual bool send(const std::vector<uint8_t>& pkt) = 0;
    virtual std::vector<uint8_t> receive() = 0;
};
