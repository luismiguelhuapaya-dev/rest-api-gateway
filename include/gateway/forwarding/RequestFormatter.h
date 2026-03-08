#pragma once

#include "gateway/Common.h"
#include "gateway/transport/HttpParser.h"
#include "gateway/routing/EndpointDefinition.h"
#include "gateway/validation/JsonParser.h"
#include "gateway/transport/FrameProtocol.h"
#include <string>
#include <cstdint>
#include <unordered_map>

namespace Gateway
{

    class RequestFormatter
    {
        public:
            RequestFormatter();
            ~RequestFormatter() = default;

            bool __thiscall FormatRequestForBackend(
                _in const HttpRequest& sHttpRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out Frame& sBackendFrame
            ) const;

            bool __thiscall FormatLoginRequest(
                _in const HttpRequest& sHttpRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out Frame& sBackendFrame
            ) const;

            bool __thiscall FormatTokenRefreshRequest(
                _in const std::string& szRefreshToken,
                _in const std::string& szServerIdentifier,
                _out Frame& sBackendFrame
            ) const;

        private:
            bool __thiscall BuildJsonPayload(
                _in const HttpRequest& sHttpRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out std::string& szJsonPayload
            ) const;

            std::string __thiscall EscapeJsonString(
                _in const std::string& szInput
            ) const;

            uint32_t        m_un32NextRequestIdentifier;
            std::mutex      m_stdMutexRequestIdentifier;
    };

} // namespace Gateway
