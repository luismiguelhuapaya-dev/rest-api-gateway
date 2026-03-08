#pragma once

#include "gateway/Common.h"
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <string_view>

namespace Gateway
{

    struct HttpRequest
    {
        HttpMethod                                      m_eMethod;
        std::string                                     m_szPath;
        std::string                                     m_szQueryString;
        std::string                                     m_szHttpVersion;
        std::unordered_map<std::string, std::string>    m_stdszszHeaders;
        std::string                                     m_szBody;
        std::unordered_map<std::string, std::string>    m_stdszszQueryParameters;
        std::unordered_map<std::string, std::string>    m_stdszszPathParameters;
        std::string                                     m_szRawRequest;
        bool                                            m_bIsComplete;
        uint32_t                                        m_un32ContentLength;
    };

    class HttpParser
    {
        public:
            HttpParser();
            ~HttpParser() = default;

            bool __thiscall Parse(
                _in const std::string& szRawData,
                _out HttpRequest& sRequest
            );

            bool __thiscall IsRequestComplete(
                _in const std::string& szRawData
            ) const;

            void __thiscall Reset();

        private:
            bool __thiscall ParseRequestLine(
                _in const std::string& szLine,
                _out HttpRequest& sRequest
            );

            bool __thiscall ParseHeaders(
                _in const std::string& szHeaderBlock,
                _out HttpRequest& sRequest
            );

            bool __thiscall ParseQueryString(
                _in const std::string& szQueryString,
                _out std::unordered_map<std::string, std::string>& stdszszParameters
            );

            std::string __thiscall UrlDecode(
                _in const std::string& szEncoded
            ) const;

            std::string __thiscall ToLowerCase(
                _in const std::string& szInput
            ) const;

            static const uint32_t       msc_un32MaxHeaderSize = 8192;
            static const uint32_t       msc_un32MaxRequestLineSize = 4096;
    };

} // namespace Gateway
