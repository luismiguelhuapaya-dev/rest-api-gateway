#pragma once

#include "gateway/Common.h"
#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>
#include <array>

namespace Gateway
{

    class Configuration
    {
        public:
            Configuration();
            ~Configuration() = default;

            Configuration(const Configuration&) = delete;
            Configuration& operator=(const Configuration&) = delete;

            bool __thiscall LoadFromFile(
                _in const std::string& szFilePath
            );

            bool __thiscall ParseCommandLineArguments(
                _in int nArgumentCount,
                _in char* apszArguments[]
            );

            bool __thiscall LoadEnvironmentVariables();

            std::string __thiscall GetTcpListenAddress() const;
            uint16_t __thiscall GetTcpListenPort() const;
            std::string __thiscall GetUnixSocketPath() const;
            uint32_t __thiscall GetMaxConnections() const;
            uint32_t __thiscall GetReadTimeoutMilliseconds() const;
            uint32_t __thiscall GetWriteTimeoutMilliseconds() const;
            uint32_t __thiscall GetMaxRequestBodySize() const;
            uint32_t __thiscall GetAccessTokenExpirySeconds() const;
            uint32_t __thiscall GetRefreshTokenExpirySeconds() const;
            LogLevel __thiscall GetLogLevel() const;
            std::string __thiscall GetLogFilePath() const;
            bool __thiscall GetLogToStdout() const;

            bool __thiscall GetAesKey(
                _out std::array<uint8_t, 32>& stdab8Key
            ) const;

            std::string __thiscall GetStringValue(
                _in const std::string& szKey,
                _in const std::string& szDefaultValue
            ) const;

            uint32_t __thiscall GetUnsignedValue(
                _in const std::string& szKey,
                _in uint32_t un32DefaultValue
            ) const;

            bool __thiscall GetBooleanValue(
                _in const std::string& szKey,
                _in bool bDefaultValue
            ) const;

        private:
            bool __thiscall ParseJsonConfiguration(
                _in const std::string& szJsonContent
            );

            bool __thiscall ParseHexString(
                _in const std::string& szHexString,
                _out std::array<uint8_t, 32>& stdab8Output
            ) const;

            std::string                                     m_szTcpListenAddress;
            uint16_t                                        m_un16TcpListenPort;
            std::string                                     m_szUnixSocketPath;
            uint32_t                                        m_un32MaxConnections;
            uint32_t                                        m_un32ReadTimeoutMilliseconds;
            uint32_t                                        m_un32WriteTimeoutMilliseconds;
            uint32_t                                        m_un32MaxRequestBodySize;
            uint32_t                                        m_un32AccessTokenExpirySeconds;
            uint32_t                                        m_un32RefreshTokenExpirySeconds;
            LogLevel                                        m_eLogLevel;
            std::string                                     m_szLogFilePath;
            bool                                            m_bLogToStdout;
            std::array<uint8_t, 32>                         m_stdab8AesKey;
            bool                                            m_bAesKeyLoaded;
            std::unordered_map<std::string, std::string>    m_stdszszConfigurationValues;
    };

} // namespace Gateway
