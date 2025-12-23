#pragma once

#include "IProtocolModule.h"
#include <vector>

class EncryptionModule : public IProtocolModule {
public:
    explicit EncryptionModule(const std::vector<uint8_t>& key);
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> reverse(const std::vector<uint8_t>& data) override;
private:
    std::vector<uint8_t> key_;
};