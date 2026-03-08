#include "gateway/logging/Logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>

namespace Gateway
{

    Logger& __stdcall Logger::GetInstance()
    {
        static Logger s_sInstance;
        return s_sInstance;
    }

    Logger::Logger()
        : m_eMinimumLevel(LogLevel::Info)
        , m_bLogToStdout(true)
        , m_bIsInitialized(false)
    {
    }

    Logger::~Logger()
    {
        Shutdown();
    }

    bool __thiscall Logger::Initialize(
        _in LogLevel eMinimumLevel,
        _in const std::string& szFilePath,
        _in bool bLogToStdout
    )
    {
        bool bResult = true;

        m_eMinimumLevel = eMinimumLevel;
        m_szFilePath = szFilePath;
        m_bLogToStdout = bLogToStdout;

        if (!szFilePath.empty())
        {
            m_stdFileStream.open(szFilePath, std::ios::app);
            if (!m_stdFileStream.is_open())
            {
                std::cerr << "Warning: Could not open log file: " << szFilePath << std::endl;
                // Continue with stdout only; this is not a fatal error
            }
        }

        m_bIsInitialized = true;

        return bResult;
    }

    void __thiscall Logger::LogMessage(
        _in LogLevel eLevel,
        _in const std::string& szComponent,
        _in const std::string& szMessage
    )
    {
        if (static_cast<int>(eLevel) >= static_cast<int>(m_eMinimumLevel))
        {
            std::string szTimestamp = FormatTimestamp();
            std::string szLevelString = LogLevelToString(eLevel);

            std::string szJsonEntry = "{";
            szJsonEntry += "\"timestamp\":\"" + szTimestamp + "\"";
            szJsonEntry += ",\"level\":\"" + szLevelString + "\"";
            szJsonEntry += ",\"component\":\"" + EscapeJsonString(szComponent) + "\"";
            szJsonEntry += ",\"message\":\"" + EscapeJsonString(szMessage) + "\"";
            szJsonEntry += "}";

            WriteLogEntry(szJsonEntry);
        }
    }

    void __thiscall Logger::LogMessage(
        _in LogLevel eLevel,
        _in const std::string& szComponent,
        _in const std::string& szMessage,
        _in const std::vector<std::pair<std::string, std::string>>& stdszszFields
    )
    {
        if (static_cast<int>(eLevel) >= static_cast<int>(m_eMinimumLevel))
        {
            std::string szTimestamp = FormatTimestamp();
            std::string szLevelString = LogLevelToString(eLevel);

            std::string szJsonEntry = "{";
            szJsonEntry += "\"timestamp\":\"" + szTimestamp + "\"";
            szJsonEntry += ",\"level\":\"" + szLevelString + "\"";
            szJsonEntry += ",\"component\":\"" + EscapeJsonString(szComponent) + "\"";
            szJsonEntry += ",\"message\":\"" + EscapeJsonString(szMessage) + "\"";

            for (const auto& stdPair : stdszszFields)
            {
                szJsonEntry += ",\"" + EscapeJsonString(stdPair.first) + "\":\"" + EscapeJsonString(stdPair.second) + "\"";
            }

            szJsonEntry += "}";

            WriteLogEntry(szJsonEntry);
        }
    }

    void __thiscall Logger::LogRequest(
        _in const std::string& szMethod,
        _in const std::string& szPath,
        _in uint32_t un32StatusCode,
        _in double fl64DurationMilliseconds,
        _in const std::string& szClientAddress
    )
    {
        std::string szTimestamp = FormatTimestamp();

        std::string szJsonEntry = "{";
        szJsonEntry += "\"timestamp\":\"" + szTimestamp + "\"";
        szJsonEntry += ",\"level\":\"INFO\"";
        szJsonEntry += ",\"component\":\"RequestLog\"";
        szJsonEntry += ",\"type\":\"request\"";
        szJsonEntry += ",\"method\":\"" + EscapeJsonString(szMethod) + "\"";
        szJsonEntry += ",\"path\":\"" + EscapeJsonString(szPath) + "\"";
        szJsonEntry += ",\"status_code\":" + std::to_string(un32StatusCode);
        szJsonEntry += ",\"duration_ms\":" + std::to_string(fl64DurationMilliseconds);
        szJsonEntry += ",\"client_address\":\"" + EscapeJsonString(szClientAddress) + "\"";
        szJsonEntry += "}";

        WriteLogEntry(szJsonEntry);
    }

    void __thiscall Logger::LogBackendEvent(
        _in const std::string& szBackendIdentifier,
        _in const std::string& szEventType,
        _in const std::string& szMessage
    )
    {
        std::string szTimestamp = FormatTimestamp();

        std::string szJsonEntry = "{";
        szJsonEntry += "\"timestamp\":\"" + szTimestamp + "\"";
        szJsonEntry += ",\"level\":\"INFO\"";
        szJsonEntry += ",\"component\":\"Backend\"";
        szJsonEntry += ",\"type\":\"backend_event\"";
        szJsonEntry += ",\"backend_id\":\"" + EscapeJsonString(szBackendIdentifier) + "\"";
        szJsonEntry += ",\"event_type\":\"" + EscapeJsonString(szEventType) + "\"";
        szJsonEntry += ",\"message\":\"" + EscapeJsonString(szMessage) + "\"";
        szJsonEntry += "}";

        WriteLogEntry(szJsonEntry);
    }

    void __thiscall Logger::LogAuthEvent(
        _in const std::string& szEventType,
        _in const std::string& szUserIdentifier,
        _in bool bSuccess,
        _in const std::string& szReason
    )
    {
        std::string szTimestamp = FormatTimestamp();

        std::string szJsonEntry = "{";
        szJsonEntry += "\"timestamp\":\"" + szTimestamp + "\"";
        szJsonEntry += ",\"level\":\"" + std::string(bSuccess ? "INFO" : "WARNING") + "\"";
        szJsonEntry += ",\"component\":\"Auth\"";
        szJsonEntry += ",\"type\":\"auth_event\"";
        szJsonEntry += ",\"event_type\":\"" + EscapeJsonString(szEventType) + "\"";
        szJsonEntry += ",\"user_id\":\"" + EscapeJsonString(szUserIdentifier) + "\"";
        szJsonEntry += ",\"success\":" + std::string(bSuccess ? "true" : "false");
        szJsonEntry += ",\"reason\":\"" + EscapeJsonString(szReason) + "\"";
        szJsonEntry += "}";

        WriteLogEntry(szJsonEntry);
    }

    void __thiscall Logger::Flush()
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexWrite);

        if (m_stdFileStream.is_open())
        {
            m_stdFileStream.flush();
        }
        if (m_bLogToStdout)
        {
            std::cout.flush();
        }
    }

    void __thiscall Logger::Shutdown()
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexWrite);

        if (m_stdFileStream.is_open())
        {
            m_stdFileStream.flush();
            m_stdFileStream.close();
        }
        m_bIsInitialized = false;
    }

    LogLevel __thiscall Logger::GetMinimumLevel() const
    {
        return m_eMinimumLevel;
    }

    void __thiscall Logger::SetMinimumLevel(
        _in LogLevel eLevel
    )
    {
        m_eMinimumLevel = eLevel;
    }

    std::string __thiscall Logger::FormatTimestamp() const
    {
        auto stdNow = std::chrono::system_clock::now();
        auto stdTimeT = std::chrono::system_clock::to_time_t(stdNow);
        auto stdDuration = stdNow.time_since_epoch();
        auto stdMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(stdDuration) % 1000;

        struct tm sTimeStruct;
        gmtime_r(&stdTimeT, &sTimeStruct);

        std::ostringstream stdStream;
        stdStream << std::put_time(&sTimeStruct, "%Y-%m-%dT%H:%M:%S")
                  << "." << std::setw(3) << std::setfill('0') << stdMilliseconds.count()
                  << "Z";

        return stdStream.str();
    }

    std::string __thiscall Logger::LogLevelToString(
        _in LogLevel eLevel
    ) const
    {
        std::string szResult = "UNKNOWN";

        if (eLevel == LogLevel::Debug)
        {
            szResult = "DEBUG";
        }
        else if (eLevel == LogLevel::Info)
        {
            szResult = "INFO";
        }
        else if (eLevel == LogLevel::Warning)
        {
            szResult = "WARNING";
        }
        else if (eLevel == LogLevel::Error)
        {
            szResult = "ERROR";
        }
        else if (eLevel == LogLevel::Fatal)
        {
            szResult = "FATAL";
        }

        return szResult;
    }

    std::string __thiscall Logger::EscapeJsonString(
        _in const std::string& szInput
    ) const
    {
        std::string szResult;
        szResult.reserve(szInput.size());

        for (char chCharacter : szInput)
        {
            if (chCharacter == '"')
            {
                szResult += "\\\"";
            }
            else if (chCharacter == '\\')
            {
                szResult += "\\\\";
            }
            else if (chCharacter == '\n')
            {
                szResult += "\\n";
            }
            else if (chCharacter == '\r')
            {
                szResult += "\\r";
            }
            else if (chCharacter == '\t')
            {
                szResult += "\\t";
            }
            else
            {
                szResult += chCharacter;
            }
        }

        return szResult;
    }

    void __thiscall Logger::WriteLogEntry(
        _in const std::string& szJsonEntry
    )
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexWrite);

        if (m_bLogToStdout)
        {
            std::cout << szJsonEntry << std::endl;
        }

        if (m_stdFileStream.is_open())
        {
            m_stdFileStream << szJsonEntry << "\n";
        }
    }

} // namespace Gateway
