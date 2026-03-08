#include "gateway/core/Configuration.h"
#include "gateway/validation/JsonParser.h"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace Gateway
{

    Configuration::Configuration()
        : m_szTcpListenAddress("0.0.0.0")
        , m_un16TcpListenPort(8080)
        , m_szUnixSocketPath("/tmp/gateway.sock")
        , m_un32MaxConnections(1024)
        , m_un32ReadTimeoutMilliseconds(30000)
        , m_un32WriteTimeoutMilliseconds(30000)
        , m_un32MaxRequestBodySize(1048576)
        , m_un32AccessTokenExpirySeconds(300)
        , m_un32RefreshTokenExpirySeconds(86400)
        , m_eLogLevel(LogLevel::Info)
        , m_szLogFilePath("/var/log/gateway.log")
        , m_bLogToStdout(true)
        , m_stdab8AesKey{}
        , m_bAesKeyLoaded(false)
    {
    }

    bool __thiscall Configuration::LoadFromFile(
        _in const std::string& szFilePath
    )
    {
        bool bResult = false;

        std::ifstream stdFileStream(szFilePath);
        if (stdFileStream.is_open())
        {
            std::string szContent(
                (std::istreambuf_iterator<char>(stdFileStream)),
                std::istreambuf_iterator<char>()
            );
            stdFileStream.close();

            if (ParseJsonConfiguration(szContent))
            {
                bResult = true;
            }
        }

        return bResult;
    }

    bool __thiscall Configuration::ParseCommandLineArguments(
        _in int nArgumentCount,
        _in char* apszArguments[]
    )
    {
        bool bResult = true;

        for (int nIndex = 1; (nIndex < nArgumentCount); ++nIndex)
        {
            std::string szArgument(apszArguments[nIndex]);

            if (((szArgument == "--config") || (szArgument == "-c")) &&
                ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                std::string szConfigPath(apszArguments[nIndex]);
                if (!LoadFromFile(szConfigPath))
                {
                    bResult = false;
                }
            }
            else if (((szArgument == "--port") || (szArgument == "-p")) &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_un16TcpListenPort = static_cast<uint16_t>(std::stoi(apszArguments[nIndex]));
            }
            else if (((szArgument == "--address") || (szArgument == "-a")) &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_szTcpListenAddress = apszArguments[nIndex];
            }
            else if (((szArgument == "--socket") || (szArgument == "-s")) &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_szUnixSocketPath = apszArguments[nIndex];
            }
            else if (((szArgument == "--max-connections") || (szArgument == "-m")) &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_un32MaxConnections = static_cast<uint32_t>(std::stoul(apszArguments[nIndex]));
            }
            else if ((szArgument == "--log-level") &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                std::string szLevel(apszArguments[nIndex]);
                if (szLevel == "debug")
                {
                    m_eLogLevel = LogLevel::Debug;
                }
                else if (szLevel == "info")
                {
                    m_eLogLevel = LogLevel::Info;
                }
                else if (szLevel == "warning")
                {
                    m_eLogLevel = LogLevel::Warning;
                }
                else if (szLevel == "error")
                {
                    m_eLogLevel = LogLevel::Error;
                }
                else if (szLevel == "fatal")
                {
                    m_eLogLevel = LogLevel::Fatal;
                }
            }
            else if ((szArgument == "--log-file") &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_szLogFilePath = apszArguments[nIndex];
            }
            else if (szArgument == "--log-stdout")
            {
                m_bLogToStdout = true;
            }
            else if (szArgument == "--no-log-stdout")
            {
                m_bLogToStdout = false;
            }
            else if ((szArgument == "--access-expiry") &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_un32AccessTokenExpirySeconds = static_cast<uint32_t>(std::stoul(apszArguments[nIndex]));
            }
            else if ((szArgument == "--refresh-expiry") &&
                     ((nIndex + 1) < nArgumentCount))
            {
                ++nIndex;
                m_un32RefreshTokenExpirySeconds = static_cast<uint32_t>(std::stoul(apszArguments[nIndex]));
            }
        }

        return bResult;
    }

    bool __thiscall Configuration::LoadEnvironmentVariables()
    {
        bool bResult = true;

        const char* pszAesKey = std::getenv("GATEWAY_AES_KEY");
        if (pszAesKey != nullptr)
        {
            std::string szAesKeyHex(pszAesKey);
            if (ParseHexString(szAesKeyHex, m_stdab8AesKey))
            {
                m_bAesKeyLoaded = true;
            }
            else
            {
                bResult = false;
            }
        }

        const char* pszPort = std::getenv("GATEWAY_PORT");
        if (pszPort != nullptr)
        {
            m_un16TcpListenPort = static_cast<uint16_t>(std::stoi(pszPort));
        }

        const char* pszAddress = std::getenv("GATEWAY_ADDRESS");
        if (pszAddress != nullptr)
        {
            m_szTcpListenAddress = pszAddress;
        }

        const char* pszSocketPath = std::getenv("GATEWAY_SOCKET_PATH");
        if (pszSocketPath != nullptr)
        {
            m_szUnixSocketPath = pszSocketPath;
        }

        const char* pszLogLevel = std::getenv("GATEWAY_LOG_LEVEL");
        if (pszLogLevel != nullptr)
        {
            std::string szLevel(pszLogLevel);
            if (szLevel == "debug")
            {
                m_eLogLevel = LogLevel::Debug;
            }
            else if (szLevel == "info")
            {
                m_eLogLevel = LogLevel::Info;
            }
            else if (szLevel == "warning")
            {
                m_eLogLevel = LogLevel::Warning;
            }
            else if (szLevel == "error")
            {
                m_eLogLevel = LogLevel::Error;
            }
            else if (szLevel == "fatal")
            {
                m_eLogLevel = LogLevel::Fatal;
            }
        }

        return bResult;
    }

    std::string __thiscall Configuration::GetTcpListenAddress() const
    {
        return m_szTcpListenAddress;
    }

    uint16_t __thiscall Configuration::GetTcpListenPort() const
    {
        return m_un16TcpListenPort;
    }

    std::string __thiscall Configuration::GetUnixSocketPath() const
    {
        return m_szUnixSocketPath;
    }

    uint32_t __thiscall Configuration::GetMaxConnections() const
    {
        return m_un32MaxConnections;
    }

    uint32_t __thiscall Configuration::GetReadTimeoutMilliseconds() const
    {
        return m_un32ReadTimeoutMilliseconds;
    }

    uint32_t __thiscall Configuration::GetWriteTimeoutMilliseconds() const
    {
        return m_un32WriteTimeoutMilliseconds;
    }

    uint32_t __thiscall Configuration::GetMaxRequestBodySize() const
    {
        return m_un32MaxRequestBodySize;
    }

    uint32_t __thiscall Configuration::GetAccessTokenExpirySeconds() const
    {
        return m_un32AccessTokenExpirySeconds;
    }

    uint32_t __thiscall Configuration::GetRefreshTokenExpirySeconds() const
    {
        return m_un32RefreshTokenExpirySeconds;
    }

    LogLevel __thiscall Configuration::GetLogLevel() const
    {
        return m_eLogLevel;
    }

    std::string __thiscall Configuration::GetLogFilePath() const
    {
        return m_szLogFilePath;
    }

    bool __thiscall Configuration::GetLogToStdout() const
    {
        return m_bLogToStdout;
    }

    bool __thiscall Configuration::GetAesKey(
        _out std::array<uint8_t, 32>& stdab8Key
    ) const
    {
        bool bResult = false;

        if (m_bAesKeyLoaded)
        {
            stdab8Key = m_stdab8AesKey;
            bResult = true;
        }

        return bResult;
    }

    std::string __thiscall Configuration::GetStringValue(
        _in const std::string& szKey,
        _in const std::string& szDefaultValue
    ) const
    {
        std::string szResult = szDefaultValue;

        auto stdIterator = m_stdszszConfigurationValues.find(szKey);
        if (stdIterator != m_stdszszConfigurationValues.end())
        {
            szResult = stdIterator->second;
        }

        return szResult;
    }

    uint32_t __thiscall Configuration::GetUnsignedValue(
        _in const std::string& szKey,
        _in uint32_t un32DefaultValue
    ) const
    {
        uint32_t un32Result = un32DefaultValue;

        auto stdIterator = m_stdszszConfigurationValues.find(szKey);
        if (stdIterator != m_stdszszConfigurationValues.end())
        {
            un32Result = static_cast<uint32_t>(std::stoul(stdIterator->second));
        }

        return un32Result;
    }

    bool __thiscall Configuration::GetBooleanValue(
        _in const std::string& szKey,
        _in bool bDefaultValue
    ) const
    {
        bool bResult = bDefaultValue;

        auto stdIterator = m_stdszszConfigurationValues.find(szKey);
        if (stdIterator != m_stdszszConfigurationValues.end())
        {
            bResult = ((stdIterator->second == "true") || (stdIterator->second == "1"));
        }

        return bResult;
    }

    bool __thiscall Configuration::ParseJsonConfiguration(
        _in const std::string& szJsonContent
    )
    {
        bool bResult = false;

        JsonParser sParser;
        JsonValue sRoot;

        if (sParser.Parse(szJsonContent, sRoot))
        {
            if (sRoot.IsObject())
            {
                if (sRoot.HasMember("listen_address"))
                {
                    m_szTcpListenAddress = sRoot.GetMember("listen_address").GetString();
                }
                if (sRoot.HasMember("listen_port"))
                {
                    m_un16TcpListenPort = static_cast<uint16_t>(sRoot.GetMember("listen_port").GetInteger());
                }
                if (sRoot.HasMember("unix_socket_path"))
                {
                    m_szUnixSocketPath = sRoot.GetMember("unix_socket_path").GetString();
                }
                if (sRoot.HasMember("max_connections"))
                {
                    m_un32MaxConnections = static_cast<uint32_t>(sRoot.GetMember("max_connections").GetInteger());
                }
                if (sRoot.HasMember("read_timeout_ms"))
                {
                    m_un32ReadTimeoutMilliseconds = static_cast<uint32_t>(sRoot.GetMember("read_timeout_ms").GetInteger());
                }
                if (sRoot.HasMember("write_timeout_ms"))
                {
                    m_un32WriteTimeoutMilliseconds = static_cast<uint32_t>(sRoot.GetMember("write_timeout_ms").GetInteger());
                }
                if (sRoot.HasMember("max_request_body_size"))
                {
                    m_un32MaxRequestBodySize = static_cast<uint32_t>(sRoot.GetMember("max_request_body_size").GetInteger());
                }
                if (sRoot.HasMember("access_token_expiry_seconds"))
                {
                    m_un32AccessTokenExpirySeconds = static_cast<uint32_t>(sRoot.GetMember("access_token_expiry_seconds").GetInteger());
                }
                if (sRoot.HasMember("refresh_token_expiry_seconds"))
                {
                    m_un32RefreshTokenExpirySeconds = static_cast<uint32_t>(sRoot.GetMember("refresh_token_expiry_seconds").GetInteger());
                }
                if (sRoot.HasMember("log_level"))
                {
                    std::string szLevel = sRoot.GetMember("log_level").GetString();
                    if (szLevel == "debug")
                    {
                        m_eLogLevel = LogLevel::Debug;
                    }
                    else if (szLevel == "info")
                    {
                        m_eLogLevel = LogLevel::Info;
                    }
                    else if (szLevel == "warning")
                    {
                        m_eLogLevel = LogLevel::Warning;
                    }
                    else if (szLevel == "error")
                    {
                        m_eLogLevel = LogLevel::Error;
                    }
                    else if (szLevel == "fatal")
                    {
                        m_eLogLevel = LogLevel::Fatal;
                    }
                }
                if (sRoot.HasMember("log_file"))
                {
                    m_szLogFilePath = sRoot.GetMember("log_file").GetString();
                }
                if (sRoot.HasMember("log_to_stdout"))
                {
                    m_bLogToStdout = sRoot.GetMember("log_to_stdout").GetBoolean();
                }
                if (sRoot.HasMember("aes_key"))
                {
                    std::string szAesKeyHex = sRoot.GetMember("aes_key").GetString();
                    if (ParseHexString(szAesKeyHex, m_stdab8AesKey))
                    {
                        m_bAesKeyLoaded = true;
                    }
                }

                bResult = true;
            }
        }

        return bResult;
    }

    bool __thiscall Configuration::ParseHexString(
        _in const std::string& szHexString,
        _out std::array<uint8_t, 32>& stdab8Output
    ) const
    {
        bool bResult = false;

        if (szHexString.size() == 64)
        {
            bResult = true;
            for (uint32_t un32Index = 0; ((un32Index < 32) && (bResult)); ++un32Index)
            {
                std::string szByte = szHexString.substr(un32Index * 2, 2);
                char* pszEnd = nullptr;
                unsigned long ulValue = std::strtoul(szByte.c_str(), &pszEnd, 16);
                if ((pszEnd == (szByte.c_str() + 2)) && (ulValue <= 255))
                {
                    stdab8Output[un32Index] = static_cast<uint8_t>(ulValue);
                }
                else
                {
                    bResult = false;
                }
            }
        }

        return bResult;
    }

} // namespace Gateway
