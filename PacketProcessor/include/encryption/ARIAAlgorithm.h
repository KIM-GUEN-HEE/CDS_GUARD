#pragma once
#include <vector>
#include <cstdint>

class ARIAAlgorithm {
public:
    explicit ARIAAlgorithm(const std::vector<uint8_t>& key);
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data);
    static constexpr size_t BLOCK_SIZE = 16;
private:
    std::vector<uint8_t> key_;
};
