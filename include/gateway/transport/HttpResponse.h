#pragma once

#include "gateway/Common.h"
#include <string>
#include <unordered_map>
#include <cstdint>

namespace Gateway
{

    class HttpResponse
    {
        public:
            HttpResponse();
            ~HttpResponse() = default;

            void __thiscall SetStatusCode(
                _in uint32_t un32StatusCode
            );

            void __thiscall SetStatusMessage(
                _in const std::string& szStatusMessage
            );

            void __thiscall SetHeader(
                _in const std::string& szName,
                _in const std::string& szValue
            );

            void __thiscall SetBody(
                _in const std::string& szBody
            );

            void __thiscall SetJsonBody(
                _in const std::string& szJsonBody
            );

            std::string __thiscall Build() const;

            uint32_t __thiscall GetStatusCode() const;

            static HttpResponse __stdcall CreateOk(
                _in const std::string& szBody
            );

            static HttpResponse __stdcall CreateBadRequest(
                _in const std::string& szMessage
            );

            static HttpResponse __stdcall CreateUnauthorized(
                _in const std::string& szMessage
            );

            static HttpResponse __stdcall CreateNotFound(
                _in const std::string& szMessage
            );

            static HttpResponse __stdcall CreateInternalError(
                _in const std::string& szMessage
            );

            static HttpResponse __stdcall CreateMethodNotAllowed(
                _in const std::string& szMessage
            );

        private:
            static std::string __stdcall GetDefaultStatusMessage(
                _in uint32_t un32StatusCode
            );

            uint32_t                                        m_un32StatusCode;
            std::string                                     m_szStatusMessage;
            std::unordered_map<std::string, std::string>    m_stdszszHeaders;
            std::string                                     m_szBody;
    };

} // namespace Gateway
