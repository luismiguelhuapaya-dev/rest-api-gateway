#include "gateway/transport/HttpParser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Gateway
{

    HttpParser::HttpParser()
    {
    }

    bool __thiscall HttpParser::Parse(
        _in const std::string& szRawData,
        _out HttpRequest& sRequest
    )
    {
        bool bResult = false;

        sRequest.m_szRawRequest = szRawData;
        sRequest.m_bIsComplete = false;
        sRequest.m_un32ContentLength = 0;
        sRequest.m_eMethod = HttpMethod::Unknown;

        // Find the end of headers
        size_t un64HeaderEnd = szRawData.find("\r\n\r\n");
        if (un64HeaderEnd != std::string::npos)
        {
            std::string szHeaderSection = szRawData.substr(0, un64HeaderEnd);
            std::string szBodySection = szRawData.substr(un64HeaderEnd + 4);

            // Parse the request line (first line)
            size_t un64FirstLineEnd = szHeaderSection.find("\r\n");
            if (un64FirstLineEnd != std::string::npos)
            {
                std::string szRequestLine = szHeaderSection.substr(0, un64FirstLineEnd);
                std::string szHeaderBlock = szHeaderSection.substr(un64FirstLineEnd + 2);

                if (ParseRequestLine(szRequestLine, sRequest))
                {
                    if (ParseHeaders(szHeaderBlock, sRequest))
                    {
                        // Check Content-Length
                        auto stdIterator = sRequest.m_stdszszHeaders.find("content-length");
                        if (stdIterator != sRequest.m_stdszszHeaders.end())
                        {
                            sRequest.m_un32ContentLength = static_cast<uint32_t>(std::stoul(stdIterator->second));
                        }

                        // Get the body
                        if (sRequest.m_un32ContentLength > 0)
                        {
                            if (szBodySection.size() >= sRequest.m_un32ContentLength)
                            {
                                sRequest.m_szBody = szBodySection.substr(0, sRequest.m_un32ContentLength);
                                sRequest.m_bIsComplete = true;
                            }
                        }
                        else
                        {
                            sRequest.m_bIsComplete = true;
                        }

                        bResult = true;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall HttpParser::IsRequestComplete(
        _in const std::string& szRawData
    ) const
    {
        bool bResult = false;

        size_t un64HeaderEnd = szRawData.find("\r\n\r\n");
        if (un64HeaderEnd != std::string::npos)
        {
            // Check if there is a content-length header
            std::string szLowerData = ToLowerCase(szRawData.substr(0, un64HeaderEnd));
            size_t un64ContentLengthPos = szLowerData.find("content-length:");
            if (un64ContentLengthPos != std::string::npos)
            {
                size_t un64ValueStart = un64ContentLengthPos + 15;
                size_t un64ValueEnd = szLowerData.find("\r\n", un64ValueStart);
                if (un64ValueEnd != std::string::npos)
                {
                    std::string szLengthValue = szRawData.substr(un64ValueStart, un64ValueEnd - un64ValueStart);
                    // Trim whitespace
                    size_t un64Start = szLengthValue.find_first_not_of(" \t");
                    if (un64Start != std::string::npos)
                    {
                        uint32_t un32ContentLength = static_cast<uint32_t>(std::stoul(szLengthValue.substr(un64Start)));
                        size_t un64BodyStart = un64HeaderEnd + 4;
                        size_t un64BodyLength = szRawData.size() - un64BodyStart;
                        if (un64BodyLength >= un32ContentLength)
                        {
                            bResult = true;
                        }
                    }
                }
            }
            else
            {
                // No content-length, complete after headers
                bResult = true;
            }
        }

        return bResult;
    }

    void __thiscall HttpParser::Reset()
    {
        // Nothing to reset for a stateless parser
    }

    bool __thiscall HttpParser::ParseRequestLine(
        _in const std::string& szLine,
        _out HttpRequest& sRequest
    )
    {
        bool bResult = false;

        // Parse "METHOD PATH HTTP/version"
        size_t un64FirstSpace = szLine.find(' ');
        if (un64FirstSpace != std::string::npos)
        {
            std::string szMethod = szLine.substr(0, un64FirstSpace);
            sRequest.m_eMethod = StringToHttpMethod(szMethod);

            size_t un64SecondSpace = szLine.find(' ', un64FirstSpace + 1);
            if (un64SecondSpace != std::string::npos)
            {
                std::string szUri = szLine.substr(un64FirstSpace + 1, un64SecondSpace - un64FirstSpace - 1);
                sRequest.m_szHttpVersion = szLine.substr(un64SecondSpace + 1);

                // Split URI into path and query
                size_t un64QueryStart = szUri.find('?');
                if (un64QueryStart != std::string::npos)
                {
                    sRequest.m_szPath = szUri.substr(0, un64QueryStart);
                    sRequest.m_szQueryString = szUri.substr(un64QueryStart + 1);
                    ParseQueryString(sRequest.m_szQueryString, sRequest.m_stdszszQueryParameters);
                }
                else
                {
                    sRequest.m_szPath = szUri;
                }

                bResult = true;
            }
        }

        return bResult;
    }

    bool __thiscall HttpParser::ParseHeaders(
        _in const std::string& szHeaderBlock,
        _out HttpRequest& sRequest
    )
    {
        bool bResult = true;

        std::istringstream stdStream(szHeaderBlock);
        std::string szLine;

        while (std::getline(stdStream, szLine))
        {
            // Remove trailing \r
            if ((!szLine.empty()) && (szLine.back() == '\r'))
            {
                szLine.pop_back();
            }

            if (!szLine.empty())
            {
                size_t un64ColonPos = szLine.find(':');
                if (un64ColonPos != std::string::npos)
                {
                    std::string szHeaderName = ToLowerCase(szLine.substr(0, un64ColonPos));
                    std::string szHeaderValue = szLine.substr(un64ColonPos + 1);

                    // Trim leading whitespace from value
                    size_t un64ValueStart = szHeaderValue.find_first_not_of(" \t");
                    if (un64ValueStart != std::string::npos)
                    {
                        szHeaderValue = szHeaderValue.substr(un64ValueStart);
                    }
                    else
                    {
                        szHeaderValue = "";
                    }

                    sRequest.m_stdszszHeaders[szHeaderName] = szHeaderValue;
                }
            }
        }

        return bResult;
    }

    bool __thiscall HttpParser::ParseQueryString(
        _in const std::string& szQueryString,
        _out std::unordered_map<std::string, std::string>& stdszszParameters
    )
    {
        bool bResult = true;

        std::string szRemaining = szQueryString;
        while (!szRemaining.empty())
        {
            std::string szPair;
            size_t un64AmpPos = szRemaining.find('&');
            if (un64AmpPos != std::string::npos)
            {
                szPair = szRemaining.substr(0, un64AmpPos);
                szRemaining = szRemaining.substr(un64AmpPos + 1);
            }
            else
            {
                szPair = szRemaining;
                szRemaining = "";
            }

            if (!szPair.empty())
            {
                size_t un64EqualsPos = szPair.find('=');
                if (un64EqualsPos != std::string::npos)
                {
                    std::string szKey = UrlDecode(szPair.substr(0, un64EqualsPos));
                    std::string szValue = UrlDecode(szPair.substr(un64EqualsPos + 1));
                    stdszszParameters[szKey] = szValue;
                }
                else
                {
                    stdszszParameters[UrlDecode(szPair)] = "";
                }
            }
        }

        return bResult;
    }

    std::string __thiscall HttpParser::UrlDecode(
        _in const std::string& szEncoded
    ) const
    {
        std::string szResult;
        szResult.reserve(szEncoded.size());

        for (size_t un64Index = 0; (un64Index < szEncoded.size()); ++un64Index)
        {
            if ((szEncoded[un64Index] == '%') &&
                ((un64Index + 2) < szEncoded.size()))
            {
                std::string szHex = szEncoded.substr(un64Index + 1, 2);
                char chDecoded = static_cast<char>(std::stoi(szHex, nullptr, 16));
                szResult += chDecoded;
                un64Index += 2;
            }
            else if (szEncoded[un64Index] == '+')
            {
                szResult += ' ';
            }
            else
            {
                szResult += szEncoded[un64Index];
            }
        }

        return szResult;
    }

    std::string __thiscall HttpParser::ToLowerCase(
        _in const std::string& szInput
    ) const
    {
        std::string szResult = szInput;
        std::transform(szResult.begin(), szResult.end(), szResult.begin(),
            [](unsigned char chCharacter)
            {
                return std::tolower(chCharacter);
            }
        );
        return szResult;
    }

} // namespace Gateway
