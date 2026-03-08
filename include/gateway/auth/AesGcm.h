#pragma once

#include "gateway/Common.h"
#include <string>
#include <cstdint>
#include <array>
#include <vector>

namespace Gateway
{

    class AesGcm
    {
        public:
            AesGcm();
            ~AesGcm();

            AesGcm(const AesGcm&) = delete;
            AesGcm& operator=(const AesGcm&) = delete;

            bool __thiscall Initialize(
                _in const std::array<uint8_t, 32>& stdab8Key
            );

            bool __thiscall Encrypt(
                _in const std::vector<uint8_t>& stdvb8Plaintext,
                _in const std::vector<uint8_t>& stdvb8AdditionalData,
                _out std::vector<uint8_t>& stdvb8Ciphertext
            ) const;

            bool __thiscall Decrypt(
                _in const std::vector<uint8_t>& stdvb8Ciphertext,
                _in const std::vector<uint8_t>& stdvb8AdditionalData,
                _out std::vector<uint8_t>& stdvb8Plaintext
            ) const;

            bool __thiscall IsInitialized() const;

        private:
            bool __thiscall GenerateRandomBytes(
                _out uint8_t* pb8Output,
                _in uint32_t un32Length
            ) const;

            std::array<uint8_t, 32>         m_stdab8Key;
            bool                            m_bIsInitialized;
            int                             m_nAlgorithmFileDescriptor;
            static const uint32_t           msc_un32IvSize = 12;
            static const uint32_t           msc_un32TagSize = 16;
            static const uint32_t           msc_un32KeySize = 32;
    };

    std::string __stdcall Base64UrlEncode(
        _in const std::vector<uint8_t>& stdvb8Data
    );

    bool __stdcall Base64UrlDecode(
        _in const std::string& szEncoded,
        _out std::vector<uint8_t>& stdvb8Data
    );

} // namespace Gateway
