#pragma once

#include <vector>
#include <cstdint>

class IProtocolModule {
public:
    virtual ~IProtocolModule() = default;
    virtual std::vector<uint8_t> process(const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> reverse(const std::vector<uint8_t>& data) = 0;
};