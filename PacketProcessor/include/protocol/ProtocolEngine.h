#pragma once

#include <vector>
#include <memory>
#include <span>
#include "IProtocolModule.h"
#include "common/Debug.h"

class ProtocolEngine {
public:
    ProtocolEngine();
    ~ProtocolEngine();
    ProtocolEngine(ProtocolEngine&&) noexcept = default;
    ProtocolEngine& operator=(ProtocolEngine&&) noexcept = default;
    ProtocolEngine(const ProtocolEngine&) = delete;
    ProtocolEngine& operator=(const ProtocolEngine&) = delete;

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) const;

    std::vector<uint8_t> encrypt(std::span<const uint8_t> data) const;
    std::vector<uint8_t> decrypt(std::span<const uint8_t> data) const;

    void addModule(std::unique_ptr<IProtocolModule> module);

private:
    std::vector<std::unique_ptr<IProtocolModule>> modules_;
};
