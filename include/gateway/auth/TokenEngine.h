#pragma once

#include "gateway/Common.h"
#include "gateway/auth/AesGcm.h"
#include <string>
#include <cstdint>
#include <array>
#include <vector>
#include <unordered_set>
#include <mutex>

namespace Gateway
{

    struct TokenPayload
    {
        std::string     m_szServerIdentifier;
        std::string     m_szUserIdentifier;
        TokenType       m_eTokenType;
        int64_t         m_n64CreationTimestamp;
        int64_t         m_n64ExpiryTimestamp;
    };

    class TokenEngine
    {
        public:
            TokenEngine();
            ~TokenEngine() = default;

            bool __thiscall Initialize(
                _in const std::array<uint8_t, 32>& stdab8Key,
                _in uint32_t un32AccessExpirySeconds,
                _in uint32_t un32RefreshExpirySeconds
            );

            bool __thiscall GenerateTokenPair(
                _in const std::string& szServerIdentifier,
                _in const std::string& szUserIdentifier,
                _out std::string& szAccessToken,
                _out std::string& szRefreshToken
            );

            bool __thiscall ValidateAccessToken(
                _in const std::string& szToken,
                _in const std::string& szExpectedServerIdentifier,
                _out std::string& szUserIdentifier
            ) const;

            bool __thiscall ValidateRefreshToken(
                _in const std::string& szToken,
                _in const std::string& szExpectedServerIdentifier,
                _out std::string& szUserIdentifier
            ) const;

            bool __thiscall RefreshTokenPair(
                _in const std::string& szRefreshToken,
                _in const std::string& szExpectedServerIdentifier,
                _out std::string& szNewAccessToken,
                _out std::string& szNewRefreshToken
            );

            bool __thiscall RevokeToken(
                _in const std::string& szToken
            );

            bool __thiscall IsTokenRevoked(
                _in const std::string& szToken
            ) const;

            uint32_t __thiscall GetActiveTokenCount() const;

            bool __thiscall IsInitialized() const;

        private:
            bool __thiscall SerializePayload(
                _in const TokenPayload& sPayload,
                _out std::vector<uint8_t>& stdvb8Serialized
            ) const;

            bool __thiscall DeserializePayload(
                _in const std::vector<uint8_t>& stdvb8Data,
                _out TokenPayload& sPayload
            ) const;

            bool __thiscall EncryptToken(
                _in const TokenPayload& sPayload,
                _out std::string& szEncryptedToken
            ) const;

            bool __thiscall DecryptToken(
                _in const std::string& szEncryptedToken,
                _out TokenPayload& sPayload
            ) const;

            int64_t __thiscall GetCurrentTimestamp() const;

            AesGcm                                  m_sAesGcm;
            uint32_t                                m_un32AccessExpirySeconds;
            uint32_t                                m_un32RefreshExpirySeconds;
            bool                                    m_bIsInitialized;
            std::unordered_set<std::string>         m_stdszRevokedTokens;
            mutable std::mutex                      m_stdMutexRevokedTokens;
            static const uint32_t                   msc_un32MaxTokenSize = 4096;
    };

} // namespace Gateway
