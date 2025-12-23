#pragma once

#include "IProtocolModule.h"
#include <vector>

class PaddingModule : public IProtocolModule {
public:
    explicit PaddingModule(const std::vector<uint8_t>& pad);
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> reverse(const std::vector<uint8_t>& data) override;

private:
    std::vector<uint8_t> pad_;
};
