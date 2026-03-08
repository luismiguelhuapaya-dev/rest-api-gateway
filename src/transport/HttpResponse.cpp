#include "gateway/transport/HttpResponse.h"
#include <sstream>

namespace Gateway
{

    HttpResponse::HttpResponse()
        : m_un32StatusCode(200)
        , m_szStatusMessage("OK")
    {
        m_stdszszHeaders["Content-Type"] = "application/json";
        m_stdszszHeaders["Connection"] = "close";
        m_stdszszHeaders["Server"] = "RestApiGateway/1.0";
    }

    void __thiscall HttpResponse::SetStatusCode(
        _in uint32_t un32StatusCode
    )
    {
        m_un32StatusCode = un32StatusCode;
        m_szStatusMessage = GetDefaultStatusMessage(un32StatusCode);
    }

    void __thiscall HttpResponse::SetStatusMessage(
        _in const std::string& szStatusMessage
    )
    {
        m_szStatusMessage = szStatusMessage;
    }

    void __thiscall HttpResponse::SetHeader(
        _in const std::string& szName,
        _in const std::string& szValue
    )
    {
        m_stdszszHeaders[szName] = szValue;
    }

    void __thiscall HttpResponse::SetBody(
        _in const std::string& szBody
    )
    {
        m_szBody = szBody;
    }

    void __thiscall HttpResponse::SetJsonBody(
        _in const std::string& szJsonBody
    )
    {
        m_szBody = szJsonBody;
        m_stdszszHeaders["Content-Type"] = "application/json";
    }

    std::string __thiscall HttpResponse::Build() const
    {
        std::ostringstream stdStream;

        stdStream << "HTTP/1.1 " << m_un32StatusCode << " " << m_szStatusMessage << "\r\n";

        for (const auto& stdPair : m_stdszszHeaders)
        {
            if (stdPair.first != "Content-Length")
            {
                stdStream << stdPair.first << ": " << stdPair.second << "\r\n";
            }
        }

        stdStream << "Content-Length: " << m_szBody.size() << "\r\n";
        stdStream << "\r\n";
        stdStream << m_szBody;

        return stdStream.str();
    }

    uint32_t __thiscall HttpResponse::GetStatusCode() const
    {
        return m_un32StatusCode;
    }

    HttpResponse __stdcall HttpResponse::CreateOk(
        _in const std::string& szBody
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(200);
        sResponse.SetJsonBody(szBody);
        return sResponse;
    }

    HttpResponse __stdcall HttpResponse::CreateBadRequest(
        _in const std::string& szMessage
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(400);
        sResponse.SetJsonBody("{\"error\":\"Bad Request\",\"message\":\"" + szMessage + "\"}");
        return sResponse;
    }

    HttpResponse __stdcall HttpResponse::CreateUnauthorized(
        _in const std::string& szMessage
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(401);
        sResponse.SetJsonBody("{\"error\":\"Unauthorized\",\"message\":\"" + szMessage + "\"}");
        return sResponse;
    }

    HttpResponse __stdcall HttpResponse::CreateNotFound(
        _in const std::string& szMessage
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(404);
        sResponse.SetJsonBody("{\"error\":\"Not Found\",\"message\":\"" + szMessage + "\"}");
        return sResponse;
    }

    HttpResponse __stdcall HttpResponse::CreateInternalError(
        _in const std::string& szMessage
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(500);
        sResponse.SetJsonBody("{\"error\":\"Internal Server Error\",\"message\":\"" + szMessage + "\"}");
        return sResponse;
    }

    HttpResponse __stdcall HttpResponse::CreateMethodNotAllowed(
        _in const std::string& szMessage
    )
    {
        HttpResponse sResponse;
        sResponse.SetStatusCode(405);
        sResponse.SetJsonBody("{\"error\":\"Method Not Allowed\",\"message\":\"" + szMessage + "\"}");
        return sResponse;
    }

    std::string __stdcall HttpResponse::GetDefaultStatusMessage(
        _in uint32_t un32StatusCode
    )
    {
        std::string szResult = "Unknown";

        if (un32StatusCode == 200)
        {
            szResult = "OK";
        }
        else if (un32StatusCode == 201)
        {
            szResult = "Created";
        }
        else if (un32StatusCode == 204)
        {
            szResult = "No Content";
        }
        else if (un32StatusCode == 400)
        {
            szResult = "Bad Request";
        }
        else if (un32StatusCode == 401)
        {
            szResult = "Unauthorized";
        }
        else if (un32StatusCode == 403)
        {
            szResult = "Forbidden";
        }
        else if (un32StatusCode == 404)
        {
            szResult = "Not Found";
        }
        else if (un32StatusCode == 405)
        {
            szResult = "Method Not Allowed";
        }
        else if (un32StatusCode == 408)
        {
            szResult = "Request Timeout";
        }
        else if (un32StatusCode == 413)
        {
            szResult = "Payload Too Large";
        }
        else if (un32StatusCode == 422)
        {
            szResult = "Unprocessable Entity";
        }
        else if (un32StatusCode == 429)
        {
            szResult = "Too Many Requests";
        }
        else if (un32StatusCode == 500)
        {
            szResult = "Internal Server Error";
        }
        else if (un32StatusCode == 502)
        {
            szResult = "Bad Gateway";
        }
        else if (un32StatusCode == 503)
        {
            szResult = "Service Unavailable";
        }
        else if (un32StatusCode == 504)
        {
            szResult = "Gateway Timeout";
        }

        return szResult;
    }

} // namespace Gateway
