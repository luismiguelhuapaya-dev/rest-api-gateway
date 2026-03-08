#include "gateway/forwarding/RequestFormatter.h"
#include "gateway/logging/Logger.h"
#include <sstream>

namespace Gateway
{

    RequestFormatter::RequestFormatter()
        : m_un32NextRequestIdentifier(1)
    {
    }

    bool __thiscall RequestFormatter::FormatRequestForBackend(
        _in const HttpRequest& sHttpRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out Frame& sBackendFrame
    ) const
    {
        bool bResult = false;

        std::string szJsonPayload;
        if (BuildJsonPayload(sHttpRequest, sEndpointDefinition, szJsonPayload))
        {
            sBackendFrame.m_eFrameType = FrameType::Request;
            sBackendFrame.m_szPayload = szJsonPayload;
            sBackendFrame.m_un32PayloadLength = static_cast<uint32_t>(szJsonPayload.size());

            // Thread-safe request ID generation
            uint32_t un32RequestId = 0;
            {
                std::lock_guard<std::mutex> stdLock(const_cast<std::mutex&>(m_stdMutexRequestIdentifier));
                un32RequestId = const_cast<uint32_t&>(m_un32NextRequestIdentifier)++;
            }
            sBackendFrame.m_un32RequestIdentifier = un32RequestId;

            bResult = true;
        }

        return bResult;
    }

    bool __thiscall RequestFormatter::FormatLoginRequest(
        _in const HttpRequest& sHttpRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out Frame& sBackendFrame
    ) const
    {
        bool bResult = false;

        std::string szJsonPayload;
        if (BuildJsonPayload(sHttpRequest, sEndpointDefinition, szJsonPayload))
        {
            sBackendFrame.m_eFrameType = FrameType::Request;
            sBackendFrame.m_szPayload = szJsonPayload;
            sBackendFrame.m_un32PayloadLength = static_cast<uint32_t>(szJsonPayload.size());

            uint32_t un32RequestId = 0;
            {
                std::lock_guard<std::mutex> stdLock(const_cast<std::mutex&>(m_stdMutexRequestIdentifier));
                un32RequestId = const_cast<uint32_t&>(m_un32NextRequestIdentifier)++;
            }
            sBackendFrame.m_un32RequestIdentifier = un32RequestId;

            bResult = true;
        }

        return bResult;
    }

    bool __thiscall RequestFormatter::FormatTokenRefreshRequest(
        _in const std::string& szRefreshToken,
        _in const std::string& szServerIdentifier,
        _out Frame& sBackendFrame
    ) const
    {
        bool bResult = true;

        std::string szJsonPayload = "{";
        szJsonPayload += "\"action\":\"token_refresh\",";
        szJsonPayload += "\"refresh_token\":\"" + EscapeJsonString(szRefreshToken) + "\",";
        szJsonPayload += "\"server_id\":\"" + EscapeJsonString(szServerIdentifier) + "\"";
        szJsonPayload += "}";

        sBackendFrame.m_eFrameType = FrameType::Request;
        sBackendFrame.m_szPayload = szJsonPayload;
        sBackendFrame.m_un32PayloadLength = static_cast<uint32_t>(szJsonPayload.size());

        uint32_t un32RequestId = 0;
        {
            std::lock_guard<std::mutex> stdLock(const_cast<std::mutex&>(m_stdMutexRequestIdentifier));
            un32RequestId = const_cast<uint32_t&>(m_un32NextRequestIdentifier)++;
        }
        sBackendFrame.m_un32RequestIdentifier = un32RequestId;

        return bResult;
    }

    bool __thiscall RequestFormatter::BuildJsonPayload(
        _in const HttpRequest& sHttpRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out std::string& szJsonPayload
    ) const
    {
        bool bResult = true;

        std::ostringstream stdStream;
        stdStream << "{";

        // Method
        stdStream << "\"method\":\"" << EscapeJsonString(HttpMethodToString(sHttpRequest.m_eMethod)) << "\"";

        // Path
        stdStream << ",\"path\":\"" << EscapeJsonString(sHttpRequest.m_szPath) << "\"";

        // Backend identifier
        stdStream << ",\"backend\":\"" << EscapeJsonString(sEndpointDefinition.m_szBackendIdentifier) << "\"";

        // Path parameters
        if (!sHttpRequest.m_stdszszPathParameters.empty())
        {
            stdStream << ",\"path_parameters\":{";
            bool bFirst = true;
            for (const auto& stdPair : sHttpRequest.m_stdszszPathParameters)
            {
                if (!bFirst)
                {
                    stdStream << ",";
                }
                bFirst = false;
                stdStream << "\"" << EscapeJsonString(stdPair.first)
                          << "\":\"" << EscapeJsonString(stdPair.second) << "\"";
            }
            stdStream << "}";
        }

        // Query parameters
        if (!sHttpRequest.m_stdszszQueryParameters.empty())
        {
            stdStream << ",\"query_parameters\":{";
            bool bFirst = true;
            for (const auto& stdPair : sHttpRequest.m_stdszszQueryParameters)
            {
                if (!bFirst)
                {
                    stdStream << ",";
                }
                bFirst = false;
                stdStream << "\"" << EscapeJsonString(stdPair.first)
                          << "\":\"" << EscapeJsonString(stdPair.second) << "\"";
            }
            stdStream << "}";
        }

        // Headers (selected)
        stdStream << ",\"headers\":{";
        bool bFirstHeader = true;
        for (const auto& stdPair : sHttpRequest.m_stdszszHeaders)
        {
            // Forward relevant headers
            if ((stdPair.first == "content-type") ||
                (stdPair.first == "accept") ||
                (stdPair.first == "user-agent") ||
                (stdPair.first == "x-request-id") ||
                (stdPair.first == "x-forwarded-for"))
            {
                if (!bFirstHeader)
                {
                    stdStream << ",";
                }
                bFirstHeader = false;
                stdStream << "\"" << EscapeJsonString(stdPair.first)
                          << "\":\"" << EscapeJsonString(stdPair.second) << "\"";
            }
        }
        stdStream << "}";

        // Body (if present)
        if (!sHttpRequest.m_szBody.empty())
        {
            // If the body is valid JSON, include it as-is
            JsonParser sParser;
            JsonValue sBodyValue;
            if (sParser.Parse(sHttpRequest.m_szBody, sBodyValue))
            {
                stdStream << ",\"body\":" << sBodyValue.Serialize();
            }
            else
            {
                stdStream << ",\"body\":\"" << EscapeJsonString(sHttpRequest.m_szBody) << "\"";
            }
        }

        stdStream << "}";
        szJsonPayload = stdStream.str();

        return bResult;
    }

    std::string __thiscall RequestFormatter::EscapeJsonString(
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

} // namespace Gateway
