#include "encryption/ARIAAlgorithm.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <random>

static std::vector<uint8_t> generate_random_bytes(size_t len) {
    std::vector<uint8_t> v(len);
    std::random_device rd;
    for (size_t i = 0; i < len; ++i) {
        v[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
    return v;
}

int main() {
    std::vector<uint8_t> key = generate_random_bytes(16);
    ARIAAlgorithm aria(key);

    std::vector<std::vector<uint8_t>> test_plaintexts;
    test_plaintexts.push_back({});  
    test_plaintexts.push_back(std::vector<uint8_t>(1, 0x00));
    test_plaintexts.push_back(std::vector<uint8_t>(ARIAAlgorithm::BLOCK_SIZE, 0x41));
    test_plaintexts.push_back(std::vector<uint8_t>(30, 0x55));
    test_plaintexts.push_back(generate_random_bytes(100));
    test_plaintexts.push_back(generate_random_bytes(1024));

    bool all_pass = true;
    for (size_t idx = 0; idx < test_plaintexts.size(); ++idx) {
        const auto& pt = test_plaintexts[idx];
        std::cout << "[Test " << idx << "] Plaintext size = " << pt.size() << " bytes... ";
        std::vector<uint8_t> cipher = aria.encrypt(pt);
        if (cipher.size() < ARIAAlgorithm::BLOCK_SIZE * 2) {
            std::cerr << "FAIL: 암호문 크기가 너무 작음 (" << cipher.size() << ")\n";
            all_pass = false;
            continue;
        }
        std::vector<uint8_t> recovered = aria.decrypt(cipher);
        if (recovered != pt) {
            std::cerr << "FAIL: 복호화 결과가 원본과 다름\n";
            std::cerr << "  원본 크기=" << pt.size() << ", 복호화 후 크기=" << recovered.size() << "\n";
            all_pass = false;
        } else {
            std::cout << "PASS\n";
        }
    }

    if (!all_pass) {
        std::cerr << "ARIAAlgorithm CBC 테스트 중 실패 케이스 존재\n";
        return 1;
    }
    std::cout << "ARIAAlgorithm CBC 모든 테스트 통과\n";
    return 0;
}
