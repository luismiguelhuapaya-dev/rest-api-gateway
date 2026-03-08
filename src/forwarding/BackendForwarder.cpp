#include "gateway/forwarding/BackendForwarder.h"
#include "gateway/logging/Logger.h"
#include "gateway/validation/JsonParser.h"
#include <chrono>

namespace Gateway
{

    BackendForwarder::BackendForwarder(
        _in EventLoop& sEventLoop,
        _in UnixSocketListener& sUnixSocketListener,
        _in TokenEngine& sTokenEngine
    )
        : m_sEventLoop(sEventLoop)
        , m_sUnixSocketListener(sUnixSocketListener)
        , m_sTokenEngine(sTokenEngine)
    {
    }

    Task<HttpResponse> __thiscall BackendForwarder::ForwardRequest(
        _in const HttpRequest& sHttpRequest,
        _in const EndpointDefinition& sEndpointDefinition
    )
    {
        HttpResponse sResponse;

        std::string szBackendIdentifier = sEndpointDefinition.m_szBackendIdentifier;

        if (m_sUnixSocketListener.HasBackend(szBackendIdentifier))
        {
            int nBackendFileDescriptor = m_sUnixSocketListener.GetBackendFileDescriptor(szBackendIdentifier);

            Frame sRequestFrame;
            if (m_sRequestFormatter.FormatRequestForBackend(sHttpRequest, sEndpointDefinition, sRequestFrame))
            {
                sResponse = co_await SendAndReceive(nBackendFileDescriptor, sRequestFrame);
            }
            else
            {
                sResponse = HttpResponse::CreateInternalError("Failed to format request for backend");
            }
        }
        else
        {
            sResponse = HttpResponse::CreateInternalError("Backend not available: " + szBackendIdentifier);
            GATEWAY_LOG_ERROR("BackendForwarder", "Backend not found: " + szBackendIdentifier);
        }

        co_return sResponse;
    }

    Task<HttpResponse> __thiscall BackendForwarder::ForwardLoginRequest(
        _in const HttpRequest& sHttpRequest,
        _in const EndpointDefinition& sEndpointDefinition
    )
    {
        HttpResponse sResponse;

        std::string szBackendIdentifier = sEndpointDefinition.m_szBackendIdentifier;

        if (m_sUnixSocketListener.HasBackend(szBackendIdentifier))
        {
            int nBackendFileDescriptor = m_sUnixSocketListener.GetBackendFileDescriptor(szBackendIdentifier);

            Frame sRequestFrame;
            if (m_sRequestFormatter.FormatLoginRequest(sHttpRequest, sEndpointDefinition, sRequestFrame))
            {
                // Login uses two-frame response handling (GW-009)
                sResponse = co_await HandleLoginResponse(nBackendFileDescriptor, sRequestFrame, sEndpointDefinition);
            }
            else
            {
                sResponse = HttpResponse::CreateInternalError("Failed to format login request");
            }
        }
        else
        {
            sResponse = HttpResponse::CreateInternalError("Backend not available: " + szBackendIdentifier);
        }

        co_return sResponse;
    }

    Task<HttpResponse> __thiscall BackendForwarder::ForwardTokenRefreshRequest(
        _in const std::string& szRefreshToken,
        _in const std::string& szServerIdentifier
    )
    {
        HttpResponse sResponse;

        // Token refresh is handled locally by the gateway (GW-010)
        std::string szNewAccessToken;
        std::string szNewRefreshToken;

        if (m_sTokenEngine.RefreshTokenPair(szRefreshToken, szServerIdentifier, szNewAccessToken, szNewRefreshToken))
        {
            std::string szResponseBody = "{";
            szResponseBody += "\"access_token\":\"" + szNewAccessToken + "\",";
            szResponseBody += "\"refresh_token\":\"" + szNewRefreshToken + "\",";
            szResponseBody += "\"token_type\":\"Bearer\"";
            szResponseBody += "}";

            sResponse = HttpResponse::CreateOk(szResponseBody);

            Logger::GetInstance().LogAuthEvent("token_refresh", "", true, "Token pair refreshed");
        }
        else
        {
            sResponse = HttpResponse::CreateUnauthorized("Invalid or expired refresh token");
            Logger::GetInstance().LogAuthEvent("token_refresh", "", false, "Invalid refresh token");
        }

        co_return sResponse;
    }

    Task<HttpResponse> __thiscall BackendForwarder::SendAndReceive(
        _in int nBackendFileDescriptor,
        _in const Frame& sRequestFrame
    )
    {
        HttpResponse sResponse;

        // Send the request frame to the backend
        bool bSendResult = co_await m_sFrameProtocol.SendFrame(m_sEventLoop, nBackendFileDescriptor, sRequestFrame);

        if (bSendResult)
        {
            // Get or create read buffer for this backend
            std::string szReadBuffer;
            {
                std::lock_guard<std::mutex> stdLock(m_stdMutexReadBuffers);
                szReadBuffer = m_stdsznReadBuffers[nBackendFileDescriptor];
            }

            // Receive the response frame
            Frame sResponseFrame = co_await m_sFrameProtocol.ReceiveFrame(m_sEventLoop, nBackendFileDescriptor, szReadBuffer);

            // Store remaining buffer
            {
                std::lock_guard<std::mutex> stdLock(m_stdMutexReadBuffers);
                m_stdsznReadBuffers[nBackendFileDescriptor] = szReadBuffer;
            }

            if (sResponseFrame.m_eFrameType != FrameType::Error)
            {
                sResponse = BuildHttpResponseFromFrame(sResponseFrame);
            }
            else
            {
                sResponse = HttpResponse::CreateInternalError("Backend returned an error");
            }
        }
        else
        {
            sResponse = HttpResponse::CreateInternalError("Failed to send request to backend");
        }

        co_return sResponse;
    }

    Task<HttpResponse> __thiscall BackendForwarder::HandleLoginResponse(
        _in int nBackendFileDescriptor,
        _in const Frame& sRequestFrame,
        _in const EndpointDefinition& sEndpointDefinition
    )
    {
        HttpResponse sResponse;

        // Send the login request frame to the backend
        bool bSendResult = co_await m_sFrameProtocol.SendFrame(m_sEventLoop, nBackendFileDescriptor, sRequestFrame);

        if (bSendResult)
        {
            std::string szReadBuffer;
            {
                std::lock_guard<std::mutex> stdLock(m_stdMutexReadBuffers);
                szReadBuffer = m_stdsznReadBuffers[nBackendFileDescriptor];
            }

            // Frame 1: Authentication result from the backend
            Frame sAuthFrame = co_await m_sFrameProtocol.ReceiveFrame(m_sEventLoop, nBackendFileDescriptor, szReadBuffer);

            if (sAuthFrame.m_eFrameType == FrameType::LoginResponse)
            {
                // Parse the authentication result
                JsonParser sParser;
                JsonValue sAuthResult;

                if (sParser.Parse(sAuthFrame.m_szPayload, sAuthResult))
                {
                    bool bAuthSuccess = false;
                    std::string szUserIdentifier;
                    std::string szServerIdentifier = sEndpointDefinition.m_szBackendIdentifier;

                    if ((sAuthResult.IsObject()) &&
                        (sAuthResult.HasMember("success")))
                    {
                        bAuthSuccess = sAuthResult.GetMember("success").GetBoolean();
                        if ((bAuthSuccess) && (sAuthResult.HasMember("user_id")))
                        {
                            szUserIdentifier = sAuthResult.GetMember("user_id").GetString();
                        }
                        if (sAuthResult.HasMember("server_id"))
                        {
                            szServerIdentifier = sAuthResult.GetMember("server_id").GetString();
                        }
                    }

                    if (bAuthSuccess)
                    {
                        // Generate token pair
                        std::string szAccessToken;
                        std::string szRefreshToken;

                        if (m_sTokenEngine.GenerateTokenPair(szServerIdentifier, szUserIdentifier, szAccessToken, szRefreshToken))
                        {
                            // Frame 2: Send token frame back to backend for any session bookkeeping
                            Frame sTokenFrame;
                            sTokenFrame.m_eFrameType = FrameType::TokenResponse;
                            sTokenFrame.m_un32RequestIdentifier = sRequestFrame.m_un32RequestIdentifier;

                            std::string szTokenPayload = "{";
                            szTokenPayload += "\"access_token\":\"" + szAccessToken + "\",";
                            szTokenPayload += "\"refresh_token\":\"" + szRefreshToken + "\",";
                            szTokenPayload += "\"user_id\":\"" + szUserIdentifier + "\"";
                            szTokenPayload += "}";
                            sTokenFrame.m_szPayload = szTokenPayload;
                            sTokenFrame.m_un32PayloadLength = static_cast<uint32_t>(szTokenPayload.size());

                            co_await m_sFrameProtocol.SendFrame(m_sEventLoop, nBackendFileDescriptor, sTokenFrame);

                            // Build the HTTP response with tokens
                            std::string szResponseBody = "{";
                            szResponseBody += "\"access_token\":\"" + szAccessToken + "\",";
                            szResponseBody += "\"refresh_token\":\"" + szRefreshToken + "\",";
                            szResponseBody += "\"token_type\":\"Bearer\",";
                            szResponseBody += "\"expires_in\":" + std::to_string(m_sTokenEngine.GetActiveTokenCount());
                            szResponseBody += "}";

                            sResponse = HttpResponse::CreateOk(szResponseBody);

                            Logger::GetInstance().LogAuthEvent("login", szUserIdentifier, true, "Login successful");
                        }
                        else
                        {
                            sResponse = HttpResponse::CreateInternalError("Failed to generate authentication tokens");
                        }
                    }
                    else
                    {
                        std::string szReason = "Invalid credentials";
                        if ((sAuthResult.IsObject()) && (sAuthResult.HasMember("message")))
                        {
                            szReason = sAuthResult.GetMember("message").GetString();
                        }
                        sResponse = HttpResponse::CreateUnauthorized(szReason);

                        Logger::GetInstance().LogAuthEvent("login", szUserIdentifier, false, szReason);
                    }
                }
                else
                {
                    sResponse = HttpResponse::CreateInternalError("Invalid authentication response from backend");
                }
            }
            else if (sAuthFrame.m_eFrameType == FrameType::Error)
            {
                sResponse = HttpResponse::CreateInternalError("Backend authentication error");
            }
            else
            {
                sResponse = HttpResponse::CreateInternalError("Unexpected response from backend during login");
            }

            // Store remaining buffer
            {
                std::lock_guard<std::mutex> stdLock(m_stdMutexReadBuffers);
                m_stdsznReadBuffers[nBackendFileDescriptor] = szReadBuffer;
            }
        }
        else
        {
            sResponse = HttpResponse::CreateInternalError("Failed to send login request to backend");
        }

        co_return sResponse;
    }

    HttpResponse __thiscall BackendForwarder::BuildHttpResponseFromFrame(
        _in const Frame& sResponseFrame
    ) const
    {
        HttpResponse sResponse;

        if (sResponseFrame.m_eFrameType == FrameType::Response)
        {
            // Parse the backend response JSON
            JsonParser sParser;
            JsonValue sResponseJson;

            if (sParser.Parse(sResponseFrame.m_szPayload, sResponseJson))
            {
                uint32_t un32StatusCode = 200;
                std::string szBody = sResponseFrame.m_szPayload;

                if (sResponseJson.IsObject())
                {
                    if (sResponseJson.HasMember("status_code"))
                    {
                        un32StatusCode = static_cast<uint32_t>(sResponseJson.GetMember("status_code").GetInteger());
                    }
                    if (sResponseJson.HasMember("body"))
                    {
                        JsonValue sBody = sResponseJson.GetMember("body");
                        szBody = sBody.Serialize();
                    }
                }

                sResponse.SetStatusCode(un32StatusCode);
                sResponse.SetJsonBody(szBody);
            }
            else
            {
                // If not valid JSON, return as-is
                sResponse.SetStatusCode(200);
                sResponse.SetBody(sResponseFrame.m_szPayload);
            }
        }
        else if (sResponseFrame.m_eFrameType == FrameType::Error)
        {
            sResponse = HttpResponse::CreateInternalError(sResponseFrame.m_szPayload);
        }
        else
        {
            sResponse.SetStatusCode(200);
            sResponse.SetJsonBody(sResponseFrame.m_szPayload);
        }

        return sResponse;
    }

} // namespace Gateway
