#include "encryption/ARIAAlgorithm.h"
#include "encryption/ARIAReference.h"
#include "common/Debug.h"
#include <cstring>
#include <random>
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>

ARIAAlgorithm::ARIAAlgorithm(const std::vector<uint8_t> &key)
    : key_(key)
{
    DBG_PRINT("ARIAAlgorithm created (key_len=%zu bytes)", key_.size());
}

static std::vector<uint8_t> pkcs7_pad(const std::vector<uint8_t> &data)
{
    size_t orig_len = data.size();
    size_t rem = orig_len % ARIAAlgorithm::BLOCK_SIZE;
    size_t pad_len = (rem == 0) ? ARIAAlgorithm::BLOCK_SIZE : (ARIAAlgorithm::BLOCK_SIZE - rem);

    std::vector<uint8_t> out;
    out.reserve(orig_len + pad_len);
    out.insert(out.end(), data.begin(), data.end());

    out.insert(out.end(), pad_len, static_cast<uint8_t>(pad_len));
    return out;
}

static std::vector<uint8_t> pkcs7_unpad(const std::vector<uint8_t> &data)
{
    size_t sz = data.size();
    if (sz == 0 || (sz % ARIAAlgorithm::BLOCK_SIZE) != 0)
    {
        return {};
    }
    uint8_t pad_byte = data[sz - 1];
    if (pad_byte == 0 || pad_byte > ARIAAlgorithm::BLOCK_SIZE)
    {

        return {};
    }

    for (size_t i = 0; i < pad_byte; ++i)
    {
        if (data[sz - 1 - i] != pad_byte)
        {

            return {};
        }
    }
    std::vector<uint8_t> out;
    out.insert(out.end(), data.begin(), data.begin() + (sz - pad_byte));
    return out;
}

static std::array<uint8_t, ARIAAlgorithm::BLOCK_SIZE> generate_random_iv()
{
    std::array<uint8_t, ARIAAlgorithm::BLOCK_SIZE> iv;
    std::random_device rd;
    for (size_t i = 0; i < ARIAAlgorithm::BLOCK_SIZE; ++i)
    {
        iv[i] = static_cast<uint8_t>(rd());
    }
    return iv;
}

std::vector<uint8_t> ARIAAlgorithm::encrypt(const std::vector<uint8_t> &data)
{
    DBG_PRINT("ARIAAlgorithm::encrypt (CBC) start (%zu bytes)", data.size());

    std::vector<uint8_t> padded = pkcs7_pad(data);
    size_t total_size = padded.size();
    size_t num_blocks = total_size / BLOCK_SIZE;

    int keyBits = static_cast<int>(key_.size() * 8);
    int maxRounds = (keyBits + 256) / 32;
    std::vector<Byte> roundKeys(16 * (maxRounds + 1));
    int R = EncKeySetup(reinterpret_cast<const Byte *>(key_.data()),
                        roundKeys.data(),
                        keyBits);
    DBG_PRINT("  EncKeySetup → rounds = %d", R);

    auto iv_arr = generate_random_iv();

    std::vector<uint8_t> out;
    out.reserve(BLOCK_SIZE + total_size);

    out.insert(out.end(), iv_arr.begin(), iv_arr.end());

    std::array<uint8_t, BLOCK_SIZE> prev_block;
    std::copy(iv_arr.begin(), iv_arr.end(), prev_block.begin());

    for (size_t bi = 0; bi < num_blocks; ++bi)
    {

        const uint8_t *pblock = padded.data() + bi * BLOCK_SIZE;

        uint8_t xored[ARIAAlgorithm::BLOCK_SIZE];
        for (size_t j = 0; j < BLOCK_SIZE; ++j)
        {
            xored[j] = static_cast<uint8_t>(pblock[j] ^ prev_block[j]);
        }

        uint8_t cipher_block[ARIAAlgorithm::BLOCK_SIZE];
        Crypt(reinterpret_cast<const Byte *>(xored),
              R,
              roundKeys.data(),
              reinterpret_cast<Byte *>(cipher_block));

        out.insert(out.end(), cipher_block, cipher_block + BLOCK_SIZE);

        std::copy(cipher_block, cipher_block + BLOCK_SIZE, prev_block.begin());
        DBG_PRINT("  └─ block %zu encrypted (CBC)", bi);
    }

    DBG_PRINT("ARIAAlgorithm::encrypt done, output size=%zu", out.size());
    return out;
}

std::vector<uint8_t> ARIAAlgorithm::decrypt(const std::vector<uint8_t> &data)
{
    DBG_PRINT("ARIAAlgorithm::decrypt (CBC) start (%zu bytes)", data.size());

    if (data.size() < 2 * BLOCK_SIZE)
    {
        DBG_PRINT("  decrypt input too short");
        return {};
    }
    size_t total_size = data.size();

    const uint8_t *iv_ptr = data.data();
    std::array<uint8_t, BLOCK_SIZE> iv_arr;
    std::copy(iv_ptr, iv_ptr + BLOCK_SIZE, iv_arr.begin());

    size_t cipher_len = total_size - BLOCK_SIZE;
    if (cipher_len % BLOCK_SIZE != 0)
    {
        DBG_PRINT("  decrypt input not multiple of block size after IV");
        return {};
    }
    size_t num_blocks = cipher_len / BLOCK_SIZE;

    int keyBits = static_cast<int>(key_.size() * 8);
    int maxRounds = (keyBits + 256) / 32;
    std::vector<Byte> roundKeys(16 * (maxRounds + 1));
    int R = DecKeySetup(reinterpret_cast<const Byte *>(key_.data()),
                        roundKeys.data(),
                        keyBits);
    DBG_PRINT("  DecKeySetup → rounds = %d", R);

    std::vector<uint8_t> decrypted;
    decrypted.reserve(cipher_len);

    std::array<uint8_t, BLOCK_SIZE> prev_block;
    std::copy(iv_arr.begin(), iv_arr.end(), prev_block.begin());

    for (size_t bi = 0; bi < num_blocks; ++bi)
    {
        const uint8_t *cblock = data.data() + BLOCK_SIZE + bi * BLOCK_SIZE;

        uint8_t interm[ARIAAlgorithm::BLOCK_SIZE];
        Crypt(reinterpret_cast<const Byte *>(cblock),
              R,
              roundKeys.data(),
              reinterpret_cast<Byte *>(interm));

        uint8_t plain_block[ARIAAlgorithm::BLOCK_SIZE];
        for (size_t j = 0; j < BLOCK_SIZE; ++j)
        {
            plain_block[j] = static_cast<uint8_t>(interm[j] ^ prev_block[j]);
        }

        decrypted.insert(decrypted.end(), plain_block, plain_block + BLOCK_SIZE);

        std::copy(cblock, cblock + BLOCK_SIZE, prev_block.begin());
        DBG_PRINT("  └─ block %zu decrypted (CBC)", bi);
    }

    std::vector<uint8_t> unpadded = pkcs7_unpad(decrypted);
    if (unpadded.empty() && !decrypted.empty())
    {
        DBG_PRINT("  padding invalid or zero-length after unpad");
        return {};
    }

    DBG_PRINT("ARIAAlgorithm::decrypt done, output size=%zu", unpadded.size());
    return unpadded;
}
