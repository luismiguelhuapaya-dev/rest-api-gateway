#include "gateway/auth/TokenEngine.h"
#include "gateway/logging/Logger.h"
#include <cstring>
#include <chrono>

namespace Gateway
{

    TokenEngine::TokenEngine()
        : m_un32AccessExpirySeconds(300)
        , m_un32RefreshExpirySeconds(86400)
        , m_bIsInitialized(false)
    {
    }

    bool __thiscall TokenEngine::Initialize(
        _in const std::array<uint8_t, 32>& stdab8Key,
        _in uint32_t un32AccessExpirySeconds,
        _in uint32_t un32RefreshExpirySeconds
    )
    {
        bool bResult = false;

        m_un32AccessExpirySeconds = un32AccessExpirySeconds;
        m_un32RefreshExpirySeconds = un32RefreshExpirySeconds;

        if (m_sAesGcm.Initialize(stdab8Key))
        {
            m_bIsInitialized = true;
            bResult = true;
            GATEWAY_LOG_INFO("TokenEngine", "Token engine initialized");
        }

        return bResult;
    }

    bool __thiscall TokenEngine::GenerateTokenPair(
        _in const std::string& szServerIdentifier,
        _in const std::string& szUserIdentifier,
        _out std::string& szAccessToken,
        _out std::string& szRefreshToken
    )
    {
        bool bResult = false;

        if (m_bIsInitialized)
        {
            int64_t n64CurrentTime = GetCurrentTimestamp();

            // Generate access token
            TokenPayload sAccessPayload;
            sAccessPayload.m_szServerIdentifier = szServerIdentifier;
            sAccessPayload.m_szUserIdentifier = szUserIdentifier;
            sAccessPayload.m_eTokenType = TokenType::Access;
            sAccessPayload.m_n64CreationTimestamp = n64CurrentTime;
            sAccessPayload.m_n64ExpiryTimestamp = n64CurrentTime + static_cast<int64_t>(m_un32AccessExpirySeconds);

            // Generate refresh token
            TokenPayload sRefreshPayload;
            sRefreshPayload.m_szServerIdentifier = szServerIdentifier;
            sRefreshPayload.m_szUserIdentifier = szUserIdentifier;
            sRefreshPayload.m_eTokenType = TokenType::Refresh;
            sRefreshPayload.m_n64CreationTimestamp = n64CurrentTime;
            sRefreshPayload.m_n64ExpiryTimestamp = n64CurrentTime + static_cast<int64_t>(m_un32RefreshExpirySeconds);

            if ((EncryptToken(sAccessPayload, szAccessToken)) &&
                (EncryptToken(sRefreshPayload, szRefreshToken)))
            {
                bResult = true;
                GATEWAY_LOG_DEBUG("TokenEngine", "Generated token pair for user: " + szUserIdentifier);
            }
        }

        return bResult;
    }

    bool __thiscall TokenEngine::ValidateAccessToken(
        _in const std::string& szToken,
        _in const std::string& szExpectedServerIdentifier,
        _out std::string& szUserIdentifier
    ) const
    {
        bool bResult = false;

        if ((m_bIsInitialized) && (!IsTokenRevoked(szToken)))
        {
            if (szToken.size() <= msc_un32MaxTokenSize)
            {
                TokenPayload sPayload;
                if (DecryptToken(szToken, sPayload))
                {
                    int64_t n64CurrentTime = GetCurrentTimestamp();

                    if ((sPayload.m_eTokenType == TokenType::Access) &&
                        (sPayload.m_szServerIdentifier == szExpectedServerIdentifier) &&
                        (sPayload.m_n64ExpiryTimestamp > n64CurrentTime))
                    {
                        szUserIdentifier = sPayload.m_szUserIdentifier;
                        bResult = true;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall TokenEngine::ValidateRefreshToken(
        _in const std::string& szToken,
        _in const std::string& szExpectedServerIdentifier,
        _out std::string& szUserIdentifier
    ) const
    {
        bool bResult = false;

        if ((m_bIsInitialized) && (!IsTokenRevoked(szToken)))
        {
            if (szToken.size() <= msc_un32MaxTokenSize)
            {
                TokenPayload sPayload;
                if (DecryptToken(szToken, sPayload))
                {
                    int64_t n64CurrentTime = GetCurrentTimestamp();

                    if ((sPayload.m_eTokenType == TokenType::Refresh) &&
                        (sPayload.m_szServerIdentifier == szExpectedServerIdentifier) &&
                        (sPayload.m_n64ExpiryTimestamp > n64CurrentTime))
                    {
                        szUserIdentifier = sPayload.m_szUserIdentifier;
                        bResult = true;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall TokenEngine::RefreshTokenPair(
        _in const std::string& szRefreshToken,
        _in const std::string& szExpectedServerIdentifier,
        _out std::string& szNewAccessToken,
        _out std::string& szNewRefreshToken
    )
    {
        bool bResult = false;

        std::string szUserIdentifier;
        if (ValidateRefreshToken(szRefreshToken, szExpectedServerIdentifier, szUserIdentifier))
        {
            // Revoke the old refresh token (rotation)
            RevokeToken(szRefreshToken);

            // Generate new token pair
            if (GenerateTokenPair(szExpectedServerIdentifier, szUserIdentifier, szNewAccessToken, szNewRefreshToken))
            {
                bResult = true;
                GATEWAY_LOG_INFO("TokenEngine", "Token pair refreshed for user: " + szUserIdentifier);
            }
        }
        else
        {
            GATEWAY_LOG_WARNING("TokenEngine", "Invalid refresh token presented");
        }

        return bResult;
    }

    bool __thiscall TokenEngine::RevokeToken(
        _in const std::string& szToken
    )
    {
        bool bResult = true;

        {
            std::lock_guard<std::mutex> stdLock(m_stdMutexRevokedTokens);
            m_stdszRevokedTokens.insert(szToken);
        }

        return bResult;
    }

    bool __thiscall TokenEngine::IsTokenRevoked(
        _in const std::string& szToken
    ) const
    {
        bool bResult = false;

        {
            std::lock_guard<std::mutex> stdLock(m_stdMutexRevokedTokens);
            if (m_stdszRevokedTokens.find(szToken) != m_stdszRevokedTokens.end())
            {
                bResult = true;
            }
        }

        return bResult;
    }

    uint32_t __thiscall TokenEngine::GetActiveTokenCount() const
    {
        uint32_t un32Result = 0;
        {
            std::lock_guard<std::mutex> stdLock(m_stdMutexRevokedTokens);
            un32Result = static_cast<uint32_t>(m_stdszRevokedTokens.size());
        }
        return un32Result;
    }

    bool __thiscall TokenEngine::IsInitialized() const
    {
        return m_bIsInitialized;
    }

    bool __thiscall TokenEngine::SerializePayload(
        _in const TokenPayload& sPayload,
        _out std::vector<uint8_t>& stdvb8Serialized
    ) const
    {
        bool bResult = true;

        stdvb8Serialized.clear();

        // Token binary format:
        // [4 bytes: Server ID length][N bytes: Server ID]
        // [4 bytes: User ID length][N bytes: User ID]
        // [1 byte: Token type (0=access, 1=refresh)]
        // [8 bytes: Creation timestamp (Unix epoch, int64)]
        // [8 bytes: Expiry timestamp (Unix epoch, int64)]

        uint32_t un32ServerIdLength = static_cast<uint32_t>(sPayload.m_szServerIdentifier.size());
        uint32_t un32UserIdLength = static_cast<uint32_t>(sPayload.m_szUserIdentifier.size());

        // Server ID length (big-endian)
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32ServerIdLength >> 24) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32ServerIdLength >> 16) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32ServerIdLength >> 8) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>(un32ServerIdLength & 0xFF));

        // Server ID bytes
        stdvb8Serialized.insert(stdvb8Serialized.end(),
            sPayload.m_szServerIdentifier.begin(),
            sPayload.m_szServerIdentifier.end());

        // User ID length (big-endian)
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32UserIdLength >> 24) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32UserIdLength >> 16) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>((un32UserIdLength >> 8) & 0xFF));
        stdvb8Serialized.push_back(static_cast<uint8_t>(un32UserIdLength & 0xFF));

        // User ID bytes
        stdvb8Serialized.insert(stdvb8Serialized.end(),
            sPayload.m_szUserIdentifier.begin(),
            sPayload.m_szUserIdentifier.end());

        // Token type
        uint8_t b8TokenType = (sPayload.m_eTokenType == TokenType::Access) ? 0 : 1;
        stdvb8Serialized.push_back(b8TokenType);

        // Creation timestamp (big-endian int64)
        int64_t n64Creation = sPayload.m_n64CreationTimestamp;
        for (int nShift = 56; (nShift >= 0); nShift -= 8)
        {
            stdvb8Serialized.push_back(static_cast<uint8_t>((n64Creation >> nShift) & 0xFF));
        }

        // Expiry timestamp (big-endian int64)
        int64_t n64Expiry = sPayload.m_n64ExpiryTimestamp;
        for (int nShift = 56; (nShift >= 0); nShift -= 8)
        {
            stdvb8Serialized.push_back(static_cast<uint8_t>((n64Expiry >> nShift) & 0xFF));
        }

        return bResult;
    }

    bool __thiscall TokenEngine::DeserializePayload(
        _in const std::vector<uint8_t>& stdvb8Data,
        _out TokenPayload& sPayload
    ) const
    {
        bool bResult = false;

        if (stdvb8Data.size() >= 25) // Minimum: 4 + 0 + 4 + 0 + 1 + 8 + 8
        {
            uint32_t un32Offset = 0;

            // Read Server ID length
            uint32_t un32ServerIdLength =
                (static_cast<uint32_t>(stdvb8Data[un32Offset]) << 24) |
                (static_cast<uint32_t>(stdvb8Data[un32Offset + 1]) << 16) |
                (static_cast<uint32_t>(stdvb8Data[un32Offset + 2]) << 8) |
                (static_cast<uint32_t>(stdvb8Data[un32Offset + 3]));
            un32Offset += 4;

            if ((un32Offset + un32ServerIdLength + 4 + 1 + 16) <= stdvb8Data.size())
            {
                // Read Server ID
                sPayload.m_szServerIdentifier.assign(
                    stdvb8Data.begin() + un32Offset,
                    stdvb8Data.begin() + un32Offset + un32ServerIdLength
                );
                un32Offset += un32ServerIdLength;

                // Read User ID length
                uint32_t un32UserIdLength =
                    (static_cast<uint32_t>(stdvb8Data[un32Offset]) << 24) |
                    (static_cast<uint32_t>(stdvb8Data[un32Offset + 1]) << 16) |
                    (static_cast<uint32_t>(stdvb8Data[un32Offset + 2]) << 8) |
                    (static_cast<uint32_t>(stdvb8Data[un32Offset + 3]));
                un32Offset += 4;

                if ((un32Offset + un32UserIdLength + 1 + 16) <= stdvb8Data.size())
                {
                    // Read User ID
                    sPayload.m_szUserIdentifier.assign(
                        stdvb8Data.begin() + un32Offset,
                        stdvb8Data.begin() + un32Offset + un32UserIdLength
                    );
                    un32Offset += un32UserIdLength;

                    // Read Token type
                    sPayload.m_eTokenType = (stdvb8Data[un32Offset] == 0) ? TokenType::Access : TokenType::Refresh;
                    un32Offset += 1;

                    if ((un32Offset + 16) <= stdvb8Data.size())
                    {
                        // Read Creation timestamp
                        sPayload.m_n64CreationTimestamp = 0;
                        for (int nByteIndex = 0; (nByteIndex < 8); ++nByteIndex)
                        {
                            sPayload.m_n64CreationTimestamp =
                                (sPayload.m_n64CreationTimestamp << 8) |
                                static_cast<int64_t>(stdvb8Data[un32Offset + nByteIndex]);
                        }
                        un32Offset += 8;

                        // Read Expiry timestamp
                        sPayload.m_n64ExpiryTimestamp = 0;
                        for (int nByteIndex = 0; (nByteIndex < 8); ++nByteIndex)
                        {
                            sPayload.m_n64ExpiryTimestamp =
                                (sPayload.m_n64ExpiryTimestamp << 8) |
                                static_cast<int64_t>(stdvb8Data[un32Offset + nByteIndex]);
                        }

                        bResult = true;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall TokenEngine::EncryptToken(
        _in const TokenPayload& sPayload,
        _out std::string& szEncryptedToken
    ) const
    {
        bool bResult = false;

        std::vector<uint8_t> stdvb8Serialized;
        if (SerializePayload(sPayload, stdvb8Serialized))
        {
            std::vector<uint8_t> stdvb8AdditionalData; // No additional authenticated data
            std::vector<uint8_t> stdvb8Ciphertext;

            if (m_sAesGcm.Encrypt(stdvb8Serialized, stdvb8AdditionalData, stdvb8Ciphertext))
            {
                szEncryptedToken = Base64UrlEncode(stdvb8Ciphertext);
                bResult = true;
            }
        }

        return bResult;
    }

    bool __thiscall TokenEngine::DecryptToken(
        _in const std::string& szEncryptedToken,
        _out TokenPayload& sPayload
    ) const
    {
        bool bResult = false;

        std::vector<uint8_t> stdvb8Ciphertext;
        if (Base64UrlDecode(szEncryptedToken, stdvb8Ciphertext))
        {
            std::vector<uint8_t> stdvb8AdditionalData; // No additional authenticated data
            std::vector<uint8_t> stdvb8Plaintext;

            if (m_sAesGcm.Decrypt(stdvb8Ciphertext, stdvb8AdditionalData, stdvb8Plaintext))
            {
                if (DeserializePayload(stdvb8Plaintext, sPayload))
                {
                    bResult = true;
                }
            }
        }

        return bResult;
    }

    int64_t __thiscall TokenEngine::GetCurrentTimestamp() const
    {
        auto stdNow = std::chrono::system_clock::now();
        auto stdDuration = stdNow.time_since_epoch();
        int64_t n64Result = std::chrono::duration_cast<std::chrono::seconds>(stdDuration).count();
        return n64Result;
    }

} // namespace Gateway
