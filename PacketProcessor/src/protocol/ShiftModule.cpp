#include "protocol/ShiftModule.h"
#include "common/Debug.h"

std::vector<uint8_t> ShiftModule::process(const std::vector<uint8_t>& in) {
    DBG_PRINT("ShiftModule::process offset=%d", offset_);
    std::vector<uint8_t> out = in;
    for (auto& b : out) {
        b = static_cast<uint8_t>(b + offset_);
    }
    return out;
}

std::vector<uint8_t> ShiftModule::reverse(const std::vector<uint8_t>& in) {
    DBG_PRINT("ShiftModule::reverse offset=%d", offset_);
    std::vector<uint8_t> out = in;
    for (auto& b : out) {
        b = static_cast<uint8_t>(b - offset_);
    }
    return out;
}
