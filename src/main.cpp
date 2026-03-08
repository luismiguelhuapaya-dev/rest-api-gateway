#include "gateway/Common.h"
#include "gateway/core/EventLoop.h"
#include "gateway/core/Configuration.h"
#include "gateway/core/Coroutine.h"
#include "gateway/transport/TcpListener.h"
#include "gateway/transport/HttpParser.h"
#include "gateway/transport/HttpResponse.h"
#include "gateway/transport/UnixSocketListener.h"
#include "gateway/transport/FrameProtocol.h"
#include "gateway/routing/RoutingTable.h"
#include "gateway/routing/EndpointDefinition.h"
#include "gateway/validation/ValidationEngine.h"
#include "gateway/validation/JsonParser.h"
#include "gateway/auth/TokenEngine.h"
#include "gateway/forwarding/BackendForwarder.h"
#include "gateway/logging/Logger.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <chrono>

using namespace Gateway;

// Global pointer used by signal handler to stop the event loop
static EventLoop* gs_psEventLoop = nullptr;

void __stdcall HandleSignal(
    _in int nSignalNumber
)
{
    if (gs_psEventLoop != nullptr)
    {
        GATEWAY_LOG_INFO("Main", "Received signal " + std::to_string(nSignalNumber) + ", shutting down...");
        gs_psEventLoop->Stop();
    }
}

class GatewayServer
{
    public:
        GatewayServer()
            : m_sTcpListener(m_sEventLoop)
            , m_sUnixSocketListener(m_sEventLoop)
            , m_sBackendForwarder(m_sEventLoop, m_sUnixSocketListener, m_sTokenEngine)
        {
        }

        bool __thiscall Initialize(
            _in int nArgumentCount,
            _in char* apszArguments[]
        )
        {
            bool bResult = false;

            // Load environment variables first (includes AES key)
            m_sConfiguration.LoadEnvironmentVariables();

            // Parse command line arguments (may override and load config file)
            m_sConfiguration.ParseCommandLineArguments(nArgumentCount, apszArguments);

            // Initialize the logger
            Logger::GetInstance().Initialize(
                m_sConfiguration.GetLogLevel(),
                m_sConfiguration.GetLogFilePath(),
                m_sConfiguration.GetLogToStdout()
            );

            GATEWAY_LOG_INFO("Main", "Initializing Dynamic REST API Gateway Server");

            // Initialize event loop
            if (m_sEventLoop.Initialize(1024))
            {
                gs_psEventLoop = &m_sEventLoop;

                // Initialize token engine
                std::array<uint8_t, 32> stdab8AesKey{};
                if (m_sConfiguration.GetAesKey(stdab8AesKey))
                {
                    if (m_sTokenEngine.Initialize(
                            stdab8AesKey,
                            m_sConfiguration.GetAccessTokenExpirySeconds(),
                            m_sConfiguration.GetRefreshTokenExpirySeconds()))
                    {
                        GATEWAY_LOG_INFO("Main", "Token engine initialized");
                    }
                    else
                    {
                        GATEWAY_LOG_WARNING("Main", "Token engine initialization failed - auth features disabled");
                    }
                }
                else
                {
                    GATEWAY_LOG_WARNING("Main", "No AES key configured - set GATEWAY_AES_KEY env var for auth features");
                }

                // Bind and listen on TCP socket
                if (m_sTcpListener.Bind(
                        m_sConfiguration.GetTcpListenAddress(),
                        m_sConfiguration.GetTcpListenPort()))
                {
                    if (m_sTcpListener.Listen(128))
                    {
                        // Bind and listen on Unix domain socket
                        if (m_sUnixSocketListener.Bind(m_sConfiguration.GetUnixSocketPath()))
                        {
                            if (m_sUnixSocketListener.Listen(32))
                            {
                                bResult = true;
                                GATEWAY_LOG_INFO("Main", "Gateway server initialized successfully");
                            }
                        }
                    }
                }
            }

            return bResult;
        }

        void __thiscall Run()
        {
            // Set up signal handlers
            signal(SIGINT, HandleSignal);
            signal(SIGTERM, HandleSignal);
            signal(SIGPIPE, SIG_IGN); // Ignore broken pipe

            GATEWAY_LOG_INFO("Main",
                "Gateway running on " + m_sConfiguration.GetTcpListenAddress() +
                ":" + std::to_string(m_sConfiguration.GetTcpListenPort()));

            // Set up the TCP connection handler
            m_sTcpListener.SetConnectionCallback(
                [this](int nClientFileDescriptor) -> Task<void>
                {
                    co_await HandleClientConnection(nClientFileDescriptor);
                }
            );

            // Set up the Unix domain socket backend handler
            m_sUnixSocketListener.SetBackendConnectionCallback(
                [this](int nBackendFileDescriptor) -> Task<void>
                {
                    co_await HandleBackendConnection(nBackendFileDescriptor);
                }
            );

            // Start the accept loops
            auto stdTcpAcceptTask = m_sTcpListener.AcceptLoop();
            m_sEventLoop.ScheduleCoroutine(stdTcpAcceptTask.GetHandle());

            auto stdUnixAcceptTask = m_sUnixSocketListener.AcceptLoop();
            m_sEventLoop.ScheduleCoroutine(stdUnixAcceptTask.GetHandle());

            // Run the event loop
            m_sEventLoop.Run();

            GATEWAY_LOG_INFO("Main", "Gateway server shutdown complete");
        }

    private:
        Task<void> __thiscall HandleClientConnection(
            _in int nClientFileDescriptor
        )
        {
            auto stdStartTime = std::chrono::steady_clock::now();

            // Read the HTTP request
            std::string szRawRequest = co_await m_sTcpListener.ReadFromClient(
                nClientFileDescriptor,
                m_sConfiguration.GetMaxRequestBodySize()
            );

            HttpResponse sResponse;

            if (!szRawRequest.empty())
            {
                HttpParser sParser;
                HttpRequest sRequest;

                if (sParser.Parse(szRawRequest, sRequest))
                {
                    // If body is incomplete, keep reading
                    while ((!sRequest.m_bIsComplete) && (!szRawRequest.empty()))
                    {
                        std::string szMoreData = co_await m_sTcpListener.ReadFromClient(
                            nClientFileDescriptor,
                            m_sConfiguration.GetMaxRequestBodySize()
                        );

                        if (!szMoreData.empty())
                        {
                            szRawRequest += szMoreData;
                            sParser.Parse(szRawRequest, sRequest);
                        }
                        else
                        {
                            break;
                        }
                    }

                    // Route the request
                    sResponse = co_await RouteRequest(sRequest);
                }
                else
                {
                    sResponse = HttpResponse::CreateBadRequest("Malformed HTTP request");
                }

                // Calculate duration
                auto stdEndTime = std::chrono::steady_clock::now();
                double fl64DurationMs = std::chrono::duration<double, std::milli>(stdEndTime - stdStartTime).count();

                // Log the request
                Logger::GetInstance().LogRequest(
                    HttpMethodToString(sRequest.m_eMethod),
                    sRequest.m_szPath,
                    sResponse.GetStatusCode(),
                    fl64DurationMs,
                    ""
                );
            }
            else
            {
                // Empty read means connection closed
                close(nClientFileDescriptor);
                co_return;
            }

            // Send the response
            std::string szResponseData = sResponse.Build();
            co_await m_sTcpListener.WriteToClient(nClientFileDescriptor, szResponseData);

            // Close the connection
            close(nClientFileDescriptor);
        }

        Task<HttpResponse> __thiscall RouteRequest(
            _in HttpRequest& sRequest
        )
        {
            HttpResponse sResponse;

            EndpointDefinition sEndpointDefinition;
            std::unordered_map<std::string, std::string> stdszszPathParameters;

            if (m_sRoutingTable.FindEndpoint(sRequest.m_szPath, sRequest.m_eMethod, sEndpointDefinition, stdszszPathParameters))
            {
                // Store path parameters in the request
                sRequest.m_stdszszPathParameters = stdszszPathParameters;

                // Check authentication if required
                bool bAuthPassed = true;
                if (sEndpointDefinition.m_bRequiresAuthentication)
                {
                    bAuthPassed = false;
                    auto stdAuthIterator = sRequest.m_stdszszHeaders.find("authorization");
                    if (stdAuthIterator != sRequest.m_stdszszHeaders.end())
                    {
                        std::string szAuthHeader = stdAuthIterator->second;
                        if (szAuthHeader.substr(0, 7) == "Bearer ")
                        {
                            std::string szToken = szAuthHeader.substr(7);
                            std::string szUserIdentifier;

                            if (m_sTokenEngine.ValidateAccessToken(
                                    szToken,
                                    sEndpointDefinition.m_szBackendIdentifier,
                                    szUserIdentifier))
                            {
                                bAuthPassed = true;
                                // Add user ID to request headers for backend
                                sRequest.m_stdszszHeaders["x-authenticated-user"] = szUserIdentifier;
                            }
                        }
                    }

                    if (!bAuthPassed)
                    {
                        sResponse = HttpResponse::CreateUnauthorized("Valid authentication token required");
                        Logger::GetInstance().LogAuthEvent("access", "", false, "Missing or invalid token");
                        co_return sResponse;
                    }
                }

                // Validate the request against the endpoint schema
                ValidationResult sValidationResult = m_sValidationEngine.ValidateRequest(sRequest, sEndpointDefinition);

                if (sValidationResult.m_bIsValid)
                {
                    // Check if this is a token refresh request
                    if ((sRequest.m_szPath == "/auth/refresh") &&
                        (sRequest.m_eMethod == HttpMethod::Post))
                    {
                        // Parse refresh token from body
                        JsonParser sJsonParser;
                        JsonValue sJsonBody;
                        std::string szRefreshToken;

                        if (sJsonParser.Parse(sRequest.m_szBody, sJsonBody))
                        {
                            if ((sJsonBody.IsObject()) && (sJsonBody.HasMember("refresh_token")))
                            {
                                szRefreshToken = sJsonBody.GetMember("refresh_token").GetString();
                            }
                        }

                        if (!szRefreshToken.empty())
                        {
                            sResponse = co_await m_sBackendForwarder.ForwardTokenRefreshRequest(
                                szRefreshToken,
                                sEndpointDefinition.m_szBackendIdentifier
                            );
                        }
                        else
                        {
                            sResponse = HttpResponse::CreateBadRequest("Missing refresh_token in request body");
                        }
                    }
                    else
                    {
                        // Forward to the backend
                        sResponse = co_await m_sBackendForwarder.ForwardRequest(sRequest, sEndpointDefinition);
                    }
                }
                else
                {
                    sResponse = HttpResponse::CreateBadRequest(sValidationResult.m_szFormattedErrorMessage);
                }
            }
            else
            {
                sResponse = HttpResponse::CreateNotFound("No endpoint registered for " +
                    HttpMethodToString(sRequest.m_eMethod) + " " + sRequest.m_szPath);
            }

            co_return sResponse;
        }

        Task<void> __thiscall HandleBackendConnection(
            _in int nBackendFileDescriptor
        )
        {
            GATEWAY_LOG_INFO("Main", "New backend connection on fd " + std::to_string(nBackendFileDescriptor));

            FrameProtocol sFrameProtocol;
            std::string szReadBuffer;

            // First frame from backend should be registration
            bool bConnectionActive = true;
            while (bConnectionActive)
            {
                Frame sFrame = co_await sFrameProtocol.ReceiveFrame(m_sEventLoop, nBackendFileDescriptor, szReadBuffer);

                if (sFrame.m_eFrameType == FrameType::Registration)
                {
                    // Parse registration payload
                    JsonParser sParser;
                    JsonValue sRegistrationData;

                    if (sParser.Parse(sFrame.m_szPayload, sRegistrationData))
                    {
                        if (sRegistrationData.IsObject())
                        {
                            std::string szBackendIdentifier;
                            if (sRegistrationData.HasMember("backend_id"))
                            {
                                szBackendIdentifier = sRegistrationData.GetMember("backend_id").GetString();
                            }

                            if (!szBackendIdentifier.empty())
                            {
                                // Register the backend
                                m_sUnixSocketListener.RegisterBackend(nBackendFileDescriptor, szBackendIdentifier);

                                Logger::GetInstance().LogBackendEvent(szBackendIdentifier, "connected", "Backend registered");

                                // Register endpoints if provided
                                if (sRegistrationData.HasMember("endpoints"))
                                {
                                    JsonValue sEndpoints = sRegistrationData.GetMember("endpoints");
                                    if (sEndpoints.IsArray())
                                    {
                                        for (uint32_t un32Index = 0; (un32Index < sEndpoints.GetArraySize()); ++un32Index)
                                        {
                                            JsonValue sEndpointJson = sEndpoints.GetArrayElement(un32Index);
                                            std::string szEndpointJsonStr = sEndpointJson.Serialize();

                                            EndpointDefinition sDefinition;
                                            if (ParseEndpointDefinitionFromJson(szEndpointJsonStr, sDefinition))
                                            {
                                                // Ensure the backend identifier matches
                                                sDefinition.m_szBackendIdentifier = szBackendIdentifier;
                                                m_sRoutingTable.RegisterEndpoint(sDefinition);
                                            }
                                        }
                                    }
                                }

                                // Send acknowledgment
                                Frame sAckFrame;
                                sAckFrame.m_eFrameType = FrameType::Response;
                                sAckFrame.m_un32RequestIdentifier = sFrame.m_un32RequestIdentifier;
                                std::string szAckPayload = "{\"status\":\"registered\",\"backend_id\":\"" + szBackendIdentifier + "\"}";
                                sAckFrame.m_szPayload = szAckPayload;
                                sAckFrame.m_un32PayloadLength = static_cast<uint32_t>(szAckPayload.size());

                                co_await sFrameProtocol.SendFrame(m_sEventLoop, nBackendFileDescriptor, sAckFrame);
                            }
                        }
                    }
                }
                else if (sFrame.m_eFrameType == FrameType::Unregistration)
                {
                    // Parse unregistration
                    JsonParser sParser;
                    JsonValue sUnregData;

                    if (sParser.Parse(sFrame.m_szPayload, sUnregData))
                    {
                        if ((sUnregData.IsObject()) && (sUnregData.HasMember("backend_id")))
                        {
                            std::string szBackendIdentifier = sUnregData.GetMember("backend_id").GetString();

                            // Remove all endpoints for this backend (GW-011)
                            uint32_t un32RemovedCount = m_sRoutingTable.RemoveEndpointsByBackend(szBackendIdentifier);

                            // Unregister the backend
                            m_sUnixSocketListener.UnregisterBackend(nBackendFileDescriptor);

                            Logger::GetInstance().LogBackendEvent(szBackendIdentifier, "disconnected",
                                "Backend unregistered, " + std::to_string(un32RemovedCount) + " endpoints removed");

                            bConnectionActive = false;
                        }
                    }
                }
                else if (sFrame.m_eFrameType == FrameType::Heartbeat)
                {
                    // Respond to heartbeat
                    Frame sHeartbeatResponse;
                    sHeartbeatResponse.m_eFrameType = FrameType::Heartbeat;
                    sHeartbeatResponse.m_un32RequestIdentifier = sFrame.m_un32RequestIdentifier;
                    sHeartbeatResponse.m_szPayload = "{\"status\":\"alive\"}";
                    sHeartbeatResponse.m_un32PayloadLength = static_cast<uint32_t>(sHeartbeatResponse.m_szPayload.size());

                    co_await sFrameProtocol.SendFrame(m_sEventLoop, nBackendFileDescriptor, sHeartbeatResponse);
                }
                else if (sFrame.m_eFrameType == FrameType::Error)
                {
                    // Backend disconnected or error
                    GATEWAY_LOG_WARNING("Main", "Backend error/disconnect on fd " + std::to_string(nBackendFileDescriptor));

                    // Find and clean up this backend
                    auto stdBackendIds = m_sUnixSocketListener.GetRegisteredBackendIdentifiers();
                    for (const auto& szId : stdBackendIds)
                    {
                        if (m_sUnixSocketListener.GetBackendFileDescriptor(szId) == nBackendFileDescriptor)
                        {
                            m_sRoutingTable.RemoveEndpointsByBackend(szId);
                            m_sUnixSocketListener.UnregisterBackend(nBackendFileDescriptor);
                            Logger::GetInstance().LogBackendEvent(szId, "lost", "Backend connection lost");
                            break;
                        }
                    }

                    bConnectionActive = false;
                }
                else
                {
                    // Unexpected frame type during backend management phase
                    // This is normal - response frames are handled by the forwarder
                }
            }
        }

        Configuration           m_sConfiguration;
        EventLoop               m_sEventLoop;
        TcpListener             m_sTcpListener;
        UnixSocketListener      m_sUnixSocketListener;
        RoutingTable            m_sRoutingTable;
        ValidationEngine        m_sValidationEngine;
        TokenEngine             m_sTokenEngine;
        BackendForwarder        m_sBackendForwarder;
};

int main(
    _in int nArgumentCount,
    _in char* apszArguments[]
)
{
    int nExitCode = 1;

    GatewayServer sServer;

    if (sServer.Initialize(nArgumentCount, apszArguments))
    {
        sServer.Run();
        nExitCode = 0;
    }
    else
    {
        std::cerr << "Failed to initialize gateway server" << std::endl;
        std::cerr << "Usage: " << apszArguments[0] << " [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  -c, --config <file>       Configuration file path" << std::endl;
        std::cerr << "  -p, --port <port>         TCP listen port (default: 8080)" << std::endl;
        std::cerr << "  -a, --address <addr>      TCP listen address (default: 0.0.0.0)" << std::endl;
        std::cerr << "  -s, --socket <path>       Unix socket path (default: /tmp/gateway.sock)" << std::endl;
        std::cerr << "  -m, --max-connections <n>  Max connections (default: 1024)" << std::endl;
        std::cerr << "  --log-level <level>       Log level: debug|info|warning|error|fatal" << std::endl;
        std::cerr << "  --log-file <path>         Log file path" << std::endl;
        std::cerr << "  --log-stdout              Log to stdout (default)" << std::endl;
        std::cerr << "  --no-log-stdout           Disable stdout logging" << std::endl;
        std::cerr << "  --access-expiry <sec>     Access token expiry (default: 300)" << std::endl;
        std::cerr << "  --refresh-expiry <sec>    Refresh token expiry (default: 86400)" << std::endl;
        std::cerr << "Environment variables:" << std::endl;
        std::cerr << "  GATEWAY_AES_KEY           AES-256 key as 64-char hex string" << std::endl;
        std::cerr << "  GATEWAY_PORT              TCP listen port" << std::endl;
        std::cerr << "  GATEWAY_ADDRESS           TCP listen address" << std::endl;
        std::cerr << "  GATEWAY_SOCKET_PATH       Unix socket path" << std::endl;
        std::cerr << "  GATEWAY_LOG_LEVEL         Log level" << std::endl;
    }

    return nExitCode;
}

// Common.h utility function implementations
namespace Gateway
{

    HttpMethod __stdcall StringToHttpMethod(
        _in const std::string& szMethod
    )
    {
        HttpMethod eResult = HttpMethod::Unknown;

        if ((szMethod == "GET") || (szMethod == "get"))
        {
            eResult = HttpMethod::Get;
        }
        else if ((szMethod == "POST") || (szMethod == "post"))
        {
            eResult = HttpMethod::Post;
        }
        else if ((szMethod == "PUT") || (szMethod == "put"))
        {
            eResult = HttpMethod::Put;
        }
        else if ((szMethod == "DELETE") || (szMethod == "delete"))
        {
            eResult = HttpMethod::Delete;
        }
        else if ((szMethod == "PATCH") || (szMethod == "patch"))
        {
            eResult = HttpMethod::Patch;
        }
        else if ((szMethod == "HEAD") || (szMethod == "head"))
        {
            eResult = HttpMethod::Head;
        }
        else if ((szMethod == "OPTIONS") || (szMethod == "options"))
        {
            eResult = HttpMethod::Options;
        }

        return eResult;
    }

    std::string __stdcall HttpMethodToString(
        _in HttpMethod eMethod
    )
    {
        std::string szResult = "UNKNOWN";

        if (eMethod == HttpMethod::Get)
        {
            szResult = "GET";
        }
        else if (eMethod == HttpMethod::Post)
        {
            szResult = "POST";
        }
        else if (eMethod == HttpMethod::Put)
        {
            szResult = "PUT";
        }
        else if (eMethod == HttpMethod::Delete)
        {
            szResult = "DELETE";
        }
        else if (eMethod == HttpMethod::Patch)
        {
            szResult = "PATCH";
        }
        else if (eMethod == HttpMethod::Head)
        {
            szResult = "HEAD";
        }
        else if (eMethod == HttpMethod::Options)
        {
            szResult = "OPTIONS";
        }

        return szResult;
    }

} // namespace Gateway
