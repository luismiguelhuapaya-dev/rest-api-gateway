#include "gateway/auth/AesGcm.h"
#include "gateway/logging/Logger.h"
#include <sys/socket.h>
#include <linux/if_alg.h>
#include <linux/socket.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/random.h>

#ifndef SOL_ALG
#define SOL_ALG 279
#endif

#ifndef ALG_SET_KEY
#define ALG_SET_KEY 1
#endif

#ifndef ALG_SET_IV
#define ALG_SET_IV 2
#endif

#ifndef ALG_SET_OP
#define ALG_SET_OP 3
#endif

#ifndef ALG_SET_AEAD_ASSOCLEN
#define ALG_SET_AEAD_ASSOCLEN 4
#endif

#ifndef ALG_SET_AEAD_AUTHSIZE
#define ALG_SET_AEAD_AUTHSIZE 5
#endif

#ifndef ALG_OP_ENCRYPT
#define ALG_OP_ENCRYPT 0
#endif

#ifndef ALG_OP_DECRYPT
#define ALG_OP_DECRYPT 1
#endif

namespace Gateway
{

    AesGcm::AesGcm()
        : m_stdab8Key{}
        , m_bIsInitialized(false)
        , m_nAlgorithmFileDescriptor(-1)
    {
    }

    AesGcm::~AesGcm()
    {
        if (m_nAlgorithmFileDescriptor >= 0)
        {
            close(m_nAlgorithmFileDescriptor);
            m_nAlgorithmFileDescriptor = -1;
        }
    }

    bool __thiscall AesGcm::Initialize(
        _in const std::array<uint8_t, 32>& stdab8Key
    )
    {
        bool bResult = false;

        m_stdab8Key = stdab8Key;

        // Create AF_ALG socket for AEAD GCM(AES)
        m_nAlgorithmFileDescriptor = socket(AF_ALG, SOCK_SEQPACKET, 0);
        if (m_nAlgorithmFileDescriptor >= 0)
        {
            struct sockaddr_alg sAlgorithmAddress{};
            sAlgorithmAddress.salg_family = AF_ALG;
            std::strcpy(reinterpret_cast<char*>(sAlgorithmAddress.salg_type), "aead");
            std::strcpy(reinterpret_cast<char*>(sAlgorithmAddress.salg_name), "gcm(aes)");

            if (bind(m_nAlgorithmFileDescriptor,
                     reinterpret_cast<struct sockaddr*>(&sAlgorithmAddress),
                     sizeof(sAlgorithmAddress)) == 0)
            {
                // Set the encryption key
                if (setsockopt(m_nAlgorithmFileDescriptor, SOL_ALG, ALG_SET_KEY,
                               m_stdab8Key.data(), msc_un32KeySize) == 0)
                {
                    // Set the auth tag size
                    if (setsockopt(m_nAlgorithmFileDescriptor, SOL_ALG, ALG_SET_AEAD_AUTHSIZE,
                                   nullptr, msc_un32TagSize) == 0)
                    {
                        m_bIsInitialized = true;
                        bResult = true;
                        GATEWAY_LOG_INFO("AesGcm", "AES-256-GCM initialized via AF_ALG");
                    }
                    else
                    {
                        GATEWAY_LOG_ERROR("AesGcm", "Failed to set auth tag size: " + std::string(strerror(errno)));
                    }
                }
                else
                {
                    GATEWAY_LOG_ERROR("AesGcm", "Failed to set encryption key: " + std::string(strerror(errno)));
                }
            }
            else
            {
                GATEWAY_LOG_ERROR("AesGcm", "Failed to bind AF_ALG socket: " + std::string(strerror(errno)));
                close(m_nAlgorithmFileDescriptor);
                m_nAlgorithmFileDescriptor = -1;
            }
        }
        else
        {
            GATEWAY_LOG_ERROR("AesGcm", "Failed to create AF_ALG socket: " + std::string(strerror(errno)));
        }

        return bResult;
    }

    bool __thiscall AesGcm::Encrypt(
        _in const std::vector<uint8_t>& stdvb8Plaintext,
        _in const std::vector<uint8_t>& stdvb8AdditionalData,
        _out std::vector<uint8_t>& stdvb8Ciphertext
    ) const
    {
        bool bResult = false;

        if (m_bIsInitialized)
        {
            // Generate random IV
            std::array<uint8_t, 12> stdab8Iv{};
            if (GenerateRandomBytes(stdab8Iv.data(), msc_un32IvSize))
            {
                // Accept a new operation file descriptor
                int nOperationFileDescriptor = accept(m_nAlgorithmFileDescriptor, nullptr, nullptr);
                if (nOperationFileDescriptor >= 0)
                {
                    // Build control message with IV, operation type, and AAD length
                    uint32_t un32AssocLength = static_cast<uint32_t>(stdvb8AdditionalData.size());

                    // Prepare the IV control data
                    struct af_alg_iv
                    {
                        uint32_t un32IvLength;
                        uint8_t ab8Iv[12];
                    };

                    // Calculate cmsg sizes
                    uint32_t un32CmsgIvSize = CMSG_SPACE(sizeof(struct af_alg_iv));
                    uint32_t un32CmsgOpSize = CMSG_SPACE(sizeof(uint32_t));
                    uint32_t un32CmsgAssocSize = CMSG_SPACE(sizeof(uint32_t));
                    uint32_t un32TotalCmsgSize = un32CmsgIvSize + un32CmsgOpSize + un32CmsgAssocSize;

                    std::vector<uint8_t> stdvb8CmsgBuffer(un32TotalCmsgSize, 0);

                    // Prepare input data: AAD + plaintext
                    std::vector<uint8_t> stdvb8InputData;
                    stdvb8InputData.insert(stdvb8InputData.end(), stdvb8AdditionalData.begin(), stdvb8AdditionalData.end());
                    stdvb8InputData.insert(stdvb8InputData.end(), stdvb8Plaintext.begin(), stdvb8Plaintext.end());

                    struct iovec sIov;
                    sIov.iov_base = stdvb8InputData.data();
                    sIov.iov_len = stdvb8InputData.size();

                    struct msghdr sMessage{};
                    sMessage.msg_iov = &sIov;
                    sMessage.msg_iovlen = 1;
                    sMessage.msg_control = stdvb8CmsgBuffer.data();
                    sMessage.msg_controllen = un32TotalCmsgSize;

                    // Set operation: encrypt
                    struct cmsghdr* pCmsg = CMSG_FIRSTHDR(&sMessage);
                    pCmsg->cmsg_level = SOL_ALG;
                    pCmsg->cmsg_type = ALG_SET_OP;
                    pCmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
                    *reinterpret_cast<uint32_t*>(CMSG_DATA(pCmsg)) = ALG_OP_ENCRYPT;

                    // Set IV
                    pCmsg = CMSG_NXTHDR(&sMessage, pCmsg);
                    pCmsg->cmsg_level = SOL_ALG;
                    pCmsg->cmsg_type = ALG_SET_IV;
                    pCmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv));
                    struct af_alg_iv* psIv = reinterpret_cast<struct af_alg_iv*>(CMSG_DATA(pCmsg));
                    psIv->un32IvLength = msc_un32IvSize;
                    std::memcpy(psIv->ab8Iv, stdab8Iv.data(), msc_un32IvSize);

                    // Set AAD length
                    pCmsg = CMSG_NXTHDR(&sMessage, pCmsg);
                    pCmsg->cmsg_level = SOL_ALG;
                    pCmsg->cmsg_type = ALG_SET_AEAD_ASSOCLEN;
                    pCmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
                    *reinterpret_cast<uint32_t*>(CMSG_DATA(pCmsg)) = un32AssocLength;

                    // Send the data
                    ssize_t nBytesSent = sendmsg(nOperationFileDescriptor, &sMessage, 0);
                    if (nBytesSent >= 0)
                    {
                        // Read the output: AAD + ciphertext + tag
                        uint32_t un32OutputSize = un32AssocLength +
                            static_cast<uint32_t>(stdvb8Plaintext.size()) + msc_un32TagSize;
                        std::vector<uint8_t> stdvb8Output(un32OutputSize);

                        ssize_t nBytesRead = read(nOperationFileDescriptor, stdvb8Output.data(), un32OutputSize);
                        if (nBytesRead > 0)
                        {
                            // Output format: IV || ciphertext || tag (skip AAD in output)
                            stdvb8Ciphertext.clear();
                            // Prepend IV
                            stdvb8Ciphertext.insert(stdvb8Ciphertext.end(), stdab8Iv.begin(), stdab8Iv.end());
                            // Append ciphertext + tag (skip AAD prefix in output)
                            stdvb8Ciphertext.insert(stdvb8Ciphertext.end(),
                                stdvb8Output.begin() + un32AssocLength,
                                stdvb8Output.end());
                            bResult = true;
                        }
                        else
                        {
                            GATEWAY_LOG_ERROR("AesGcm", "Failed to read encrypted output");
                        }
                    }
                    else
                    {
                        GATEWAY_LOG_ERROR("AesGcm", "Failed to send data for encryption: " + std::string(strerror(errno)));
                    }

                    close(nOperationFileDescriptor);
                }
                else
                {
                    GATEWAY_LOG_ERROR("AesGcm", "Failed to accept operation fd: " + std::string(strerror(errno)));
                }
            }
            else
            {
                GATEWAY_LOG_ERROR("AesGcm", "Failed to generate random IV");
            }
        }

        return bResult;
    }

    bool __thiscall AesGcm::Decrypt(
        _in const std::vector<uint8_t>& stdvb8Ciphertext,
        _in const std::vector<uint8_t>& stdvb8AdditionalData,
        _out std::vector<uint8_t>& stdvb8Plaintext
    ) const
    {
        bool bResult = false;

        if ((m_bIsInitialized) && (stdvb8Ciphertext.size() > (msc_un32IvSize + msc_un32TagSize)))
        {
            // Extract IV from beginning of ciphertext
            std::array<uint8_t, 12> stdab8Iv{};
            std::memcpy(stdab8Iv.data(), stdvb8Ciphertext.data(), msc_un32IvSize);

            // Extract the actual ciphertext + tag
            std::vector<uint8_t> stdvb8EncryptedData(
                stdvb8Ciphertext.begin() + msc_un32IvSize,
                stdvb8Ciphertext.end()
            );

            int nOperationFileDescriptor = accept(m_nAlgorithmFileDescriptor, nullptr, nullptr);
            if (nOperationFileDescriptor >= 0)
            {
                uint32_t un32AssocLength = static_cast<uint32_t>(stdvb8AdditionalData.size());

                struct af_alg_iv
                {
                    uint32_t un32IvLength;
                    uint8_t ab8Iv[12];
                };

                uint32_t un32CmsgIvSize = CMSG_SPACE(sizeof(struct af_alg_iv));
                uint32_t un32CmsgOpSize = CMSG_SPACE(sizeof(uint32_t));
                uint32_t un32CmsgAssocSize = CMSG_SPACE(sizeof(uint32_t));
                uint32_t un32TotalCmsgSize = un32CmsgIvSize + un32CmsgOpSize + un32CmsgAssocSize;

                std::vector<uint8_t> stdvb8CmsgBuffer(un32TotalCmsgSize, 0);

                // Prepare input: AAD + ciphertext + tag
                std::vector<uint8_t> stdvb8InputData;
                stdvb8InputData.insert(stdvb8InputData.end(), stdvb8AdditionalData.begin(), stdvb8AdditionalData.end());
                stdvb8InputData.insert(stdvb8InputData.end(), stdvb8EncryptedData.begin(), stdvb8EncryptedData.end());

                struct iovec sIov;
                sIov.iov_base = stdvb8InputData.data();
                sIov.iov_len = stdvb8InputData.size();

                struct msghdr sMessage{};
                sMessage.msg_iov = &sIov;
                sMessage.msg_iovlen = 1;
                sMessage.msg_control = stdvb8CmsgBuffer.data();
                sMessage.msg_controllen = un32TotalCmsgSize;

                // Set operation: decrypt
                struct cmsghdr* pCmsg = CMSG_FIRSTHDR(&sMessage);
                pCmsg->cmsg_level = SOL_ALG;
                pCmsg->cmsg_type = ALG_SET_OP;
                pCmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
                *reinterpret_cast<uint32_t*>(CMSG_DATA(pCmsg)) = ALG_OP_DECRYPT;

                // Set IV
                pCmsg = CMSG_NXTHDR(&sMessage, pCmsg);
                pCmsg->cmsg_level = SOL_ALG;
                pCmsg->cmsg_type = ALG_SET_IV;
                pCmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv));
                struct af_alg_iv* psIv = reinterpret_cast<struct af_alg_iv*>(CMSG_DATA(pCmsg));
                psIv->un32IvLength = msc_un32IvSize;
                std::memcpy(psIv->ab8Iv, stdab8Iv.data(), msc_un32IvSize);

                // Set AAD length
                pCmsg = CMSG_NXTHDR(&sMessage, pCmsg);
                pCmsg->cmsg_level = SOL_ALG;
                pCmsg->cmsg_type = ALG_SET_AEAD_ASSOCLEN;
                pCmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
                *reinterpret_cast<uint32_t*>(CMSG_DATA(pCmsg)) = un32AssocLength;

                ssize_t nBytesSent = sendmsg(nOperationFileDescriptor, &sMessage, 0);
                if (nBytesSent >= 0)
                {
                    // Output: AAD + plaintext (tag is consumed/verified by kernel)
                    uint32_t un32OutputSize = un32AssocLength +
                        static_cast<uint32_t>(stdvb8EncryptedData.size()) - msc_un32TagSize;
                    std::vector<uint8_t> stdvb8Output(un32OutputSize);

                    ssize_t nBytesRead = read(nOperationFileDescriptor, stdvb8Output.data(), un32OutputSize);
                    if (nBytesRead > 0)
                    {
                        // Skip AAD in output to get plaintext
                        stdvb8Plaintext.assign(
                            stdvb8Output.begin() + un32AssocLength,
                            stdvb8Output.begin() + nBytesRead
                        );
                        bResult = true;
                    }
                    else
                    {
                        GATEWAY_LOG_ERROR("AesGcm", "Decryption failed - authentication tag mismatch or read error");
                    }
                }
                else
                {
                    GATEWAY_LOG_ERROR("AesGcm", "Failed to send data for decryption: " + std::string(strerror(errno)));
                }

                close(nOperationFileDescriptor);
            }
        }

        return bResult;
    }

    bool __thiscall AesGcm::IsInitialized() const
    {
        return m_bIsInitialized;
    }

    bool __thiscall AesGcm::GenerateRandomBytes(
        _out uint8_t* pb8Output,
        _in uint32_t un32Length
    ) const
    {
        bool bResult = false;

        ssize_t nBytesGenerated = getrandom(pb8Output, un32Length, 0);
        if (nBytesGenerated == static_cast<ssize_t>(un32Length))
        {
            bResult = true;
        }

        return bResult;
    }

    // Base64url encoding/decoding
    static const char gc_aszBase64UrlChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string __stdcall Base64UrlEncode(
        _in const std::vector<uint8_t>& stdvb8Data
    )
    {
        std::string szResult;
        uint32_t un32DataSize = static_cast<uint32_t>(stdvb8Data.size());

        szResult.reserve(((un32DataSize + 2) / 3) * 4);

        for (uint32_t un32Index = 0; (un32Index < un32DataSize); un32Index += 3)
        {
            uint32_t un32Triple = static_cast<uint32_t>(stdvb8Data[un32Index]) << 16;
            if ((un32Index + 1) < un32DataSize)
            {
                un32Triple |= static_cast<uint32_t>(stdvb8Data[un32Index + 1]) << 8;
            }
            if ((un32Index + 2) < un32DataSize)
            {
                un32Triple |= static_cast<uint32_t>(stdvb8Data[un32Index + 2]);
            }

            szResult += gc_aszBase64UrlChars[(un32Triple >> 18) & 0x3F];
            szResult += gc_aszBase64UrlChars[(un32Triple >> 12) & 0x3F];

            if ((un32Index + 1) < un32DataSize)
            {
                szResult += gc_aszBase64UrlChars[(un32Triple >> 6) & 0x3F];
            }

            if ((un32Index + 2) < un32DataSize)
            {
                szResult += gc_aszBase64UrlChars[un32Triple & 0x3F];
            }
        }

        return szResult;
    }

    bool __stdcall Base64UrlDecode(
        _in const std::string& szEncoded,
        _out std::vector<uint8_t>& stdvb8Data
    )
    {
        bool bResult = true;

        // Build lookup table
        std::array<int, 256> stdan32Lookup;
        stdan32Lookup.fill(-1);
        for (int nIndex = 0; (nIndex < 64); ++nIndex)
        {
            stdan32Lookup[static_cast<unsigned char>(gc_aszBase64UrlChars[nIndex])] = nIndex;
        }
        // Also support standard base64 '+' and '/'
        stdan32Lookup[static_cast<unsigned char>('+')] = 62;
        stdan32Lookup[static_cast<unsigned char>('/')] = 63;

        stdvb8Data.clear();
        stdvb8Data.reserve((szEncoded.size() * 3) / 4);

        uint32_t un32Accumulator = 0;
        uint32_t un32Bits = 0;

        for (size_t un64Index = 0; ((un64Index < szEncoded.size()) && (bResult)); ++un64Index)
        {
            char chCurrent = szEncoded[un64Index];
            if (chCurrent == '=')
            {
                break;
            }

            int nValue = stdan32Lookup[static_cast<unsigned char>(chCurrent)];
            if (nValue < 0)
            {
                bResult = false;
            }
            else
            {
                un32Accumulator = (un32Accumulator << 6) | static_cast<uint32_t>(nValue);
                un32Bits += 6;

                if (un32Bits >= 8)
                {
                    un32Bits -= 8;
                    stdvb8Data.push_back(static_cast<uint8_t>((un32Accumulator >> un32Bits) & 0xFF));
                }
            }
        }

        return bResult;
    }

} // namespace Gateway
