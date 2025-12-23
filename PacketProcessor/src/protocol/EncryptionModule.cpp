#include "protocol/EncryptionModule.h"
#include "encryption/ARIAAlgorithm.h"
#include "common/Debug.h"

EncryptionModule::EncryptionModule(const std::vector<uint8_t>& key)
  : key_(key)
{
    DBG_PRINT("EncryptionModule created (key_len=%zu)", key_.size());
}

std::vector<uint8_t> EncryptionModule::process(const std::vector<uint8_t>& data) {
    DBG_PRINT("EncryptionModule::process");
    ARIAAlgorithm algo(key_);
    return algo.encrypt(data);
}

std::vector<uint8_t> EncryptionModule::reverse(const std::vector<uint8_t>& data) {
    DBG_PRINT("EncryptionModule::reverse");
    ARIAAlgorithm algo(key_);
    return algo.decrypt(data);
}
