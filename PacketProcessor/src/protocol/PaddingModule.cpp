#include "protocol/PaddingModule.h"
#include "common/Debug.h"
#include <algorithm>

PaddingModule::PaddingModule(const std::vector<uint8_t>& pad)
  : pad_(pad)
{}

std::vector<uint8_t> PaddingModule::process(const std::vector<uint8_t>& in) {
    DBG_PRINT("PaddingModule::process pad_len=%zu", pad_.size());
    std::vector<uint8_t> out = in;
    out.insert(out.begin(), pad_.begin(), pad_.end());
    return out;
}

std::vector<uint8_t> PaddingModule::reverse(const std::vector<uint8_t>& in) {
    DBG_PRINT("PaddingModule::reverse pad_len=%zu", pad_.size());
    std::vector<uint8_t> out = in;
    if (out.size() >= pad_.size()
        && std::equal(pad_.begin(), pad_.end(), out.begin())) {
        out.erase(out.begin(), out.begin() + pad_.size());
    } else {
        DBG_PRINT("PaddingModule: padding mismatch!");
    }
    return out;
}
