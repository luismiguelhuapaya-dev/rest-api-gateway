#pragma once

#include "gateway/Common.h"
#include <string>
#include <cstdint>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>

namespace Gateway
{

    class Logger
    {
        public:
            static Logger& __stdcall GetInstance();

            bool __thiscall Initialize(
                _in LogLevel eMinimumLevel,
                _in const std::string& szFilePath,
                _in bool bLogToStdout
            );

            void __thiscall LogMessage(
                _in LogLevel eLevel,
                _in const std::string& szComponent,
                _in const std::string& szMessage
            );

            void __thiscall LogMessage(
                _in LogLevel eLevel,
                _in const std::string& szComponent,
                _in const std::string& szMessage,
                _in const std::vector<std::pair<std::string, std::string>>& stdszszFields
            );

            void __thiscall LogRequest(
                _in const std::string& szMethod,
                _in const std::string& szPath,
                _in uint32_t un32StatusCode,
                _in double fl64DurationMilliseconds,
                _in const std::string& szClientAddress
            );

            void __thiscall LogBackendEvent(
                _in const std::string& szBackendIdentifier,
                _in const std::string& szEventType,
                _in const std::string& szMessage
            );

            void __thiscall LogAuthEvent(
                _in const std::string& szEventType,
                _in const std::string& szUserIdentifier,
                _in bool bSuccess,
                _in const std::string& szReason
            );

            void __thiscall Flush();
            void __thiscall Shutdown();

            LogLevel __thiscall GetMinimumLevel() const;

            void __thiscall SetMinimumLevel(
                _in LogLevel eLevel
            );

        private:
            Logger();
            ~Logger();

            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;

            std::string __thiscall FormatTimestamp() const;

            std::string __thiscall LogLevelToString(
                _in LogLevel eLevel
            ) const;

            std::string __thiscall EscapeJsonString(
                _in const std::string& szInput
            ) const;

            void __thiscall WriteLogEntry(
                _in const std::string& szJsonEntry
            );

            LogLevel                m_eMinimumLevel;
            std::string             m_szFilePath;
            bool                    m_bLogToStdout;
            bool                    m_bIsInitialized;
            std::ofstream           m_stdFileStream;
            std::mutex              m_stdMutexWrite;
    };

} // namespace Gateway

#define GATEWAY_LOG_DEBUG(component, message) \
    Gateway::Logger::GetInstance().LogMessage(Gateway::LogLevel::Debug, component, message)

#define GATEWAY_LOG_INFO(component, message) \
    Gateway::Logger::GetInstance().LogMessage(Gateway::LogLevel::Info, component, message)

#define GATEWAY_LOG_WARNING(component, message) \
    Gateway::Logger::GetInstance().LogMessage(Gateway::LogLevel::Warning, component, message)

#define GATEWAY_LOG_ERROR(component, message) \
    Gateway::Logger::GetInstance().LogMessage(Gateway::LogLevel::Error, component, message)

#define GATEWAY_LOG_FATAL(component, message) \
    Gateway::Logger::GetInstance().LogMessage(Gateway::LogLevel::Fatal, component, message)
