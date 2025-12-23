#include "protocol/ProtocolEngine.h"

ProtocolEngine::ProtocolEngine() = default;
ProtocolEngine::~ProtocolEngine() = default;

std::vector<uint8_t> ProtocolEngine::encrypt(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> tmp = data;
    DBG_PRINT("Encrypt start (%zu bytes)", tmp.size());
    for (auto& mod : modules_) {
        tmp = mod->process(tmp);
        DBG_PRINT(" → after %s: size=%zu",
                  typeid(*mod).name(), tmp.size());
    }
    DBG_PRINT("Encrypt done");
    return tmp;
}

std::vector<uint8_t> ProtocolEngine::decrypt(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> tmp = data;
    DBG_PRINT("Decrypt start (%zu bytes)", tmp.size());
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        tmp = (*it)->reverse(tmp);
        DBG_PRINT(" ← after %s.reverse: size=%zu",
                  typeid(**it).name(), tmp.size());
    }
    DBG_PRINT("Decrypt done");
    return tmp;
}

std::vector<uint8_t> ProtocolEngine::encrypt(std::span<const uint8_t> data) const {
    std::vector<uint8_t> tmp(data.begin(), data.end());
    DBG_PRINT("Encrypt (span) start (%zu bytes)", tmp.size());

    for (auto& mod : modules_) {
        tmp = mod->process(tmp);
        DBG_PRINT(" → after %s: size=%zu",
                  typeid(*mod).name(), tmp.size());
    }

    DBG_PRINT("Encrypt (span) done");
    return tmp;
}

std::vector<uint8_t> ProtocolEngine::decrypt(std::span<const uint8_t> data) const {
    std::vector<uint8_t> tmp(data.begin(), data.end());
    DBG_PRINT("Decrypt (span) start (%zu bytes)", tmp.size());

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        tmp = (*it)->reverse(tmp);
        DBG_PRINT(" ← after %s.reverse: size=%zu",
                  typeid(**it).name(), tmp.size());
    }

    DBG_PRINT("Decrypt (span) done");
    return tmp;
}

void ProtocolEngine::addModule(std::unique_ptr<IProtocolModule> module) {
    DBG_PRINT("Added module %s", typeid(*module).name());
    modules_.push_back(std::move(module));
}
