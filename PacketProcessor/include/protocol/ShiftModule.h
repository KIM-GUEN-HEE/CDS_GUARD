#pragma once

#include "IProtocolModule.h"

class ShiftModule : public IProtocolModule {
public:
    explicit ShiftModule(int offset) : offset_(offset) {}
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> reverse(const std::vector<uint8_t>& data) override;

private:
    int offset_;
};
