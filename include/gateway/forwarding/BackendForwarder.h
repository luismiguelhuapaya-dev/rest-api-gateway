#pragma once

#include "gateway/Common.h"
#include "gateway/core/EventLoop.h"
#include "gateway/core/Coroutine.h"
#include "gateway/transport/FrameProtocol.h"
#include "gateway/transport/UnixSocketListener.h"
#include "gateway/transport/HttpParser.h"
#include "gateway/transport/HttpResponse.h"
#include "gateway/routing/EndpointDefinition.h"
#include "gateway/forwarding/RequestFormatter.h"
#include "gateway/auth/TokenEngine.h"
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Gateway
{

    class BackendForwarder
    {
        public:
            BackendForwarder(
                _in EventLoop& sEventLoop,
                _in UnixSocketListener& sUnixSocketListener,
                _in TokenEngine& sTokenEngine
            );
            ~BackendForwarder() = default;

            BackendForwarder(const BackendForwarder&) = delete;
            BackendForwarder& operator=(const BackendForwarder&) = delete;

            Task<HttpResponse> __thiscall ForwardRequest(
                _in const HttpRequest& sHttpRequest,
                _in const EndpointDefinition& sEndpointDefinition
            );

            Task<HttpResponse> __thiscall ForwardLoginRequest(
                _in const HttpRequest& sHttpRequest,
                _in const EndpointDefinition& sEndpointDefinition
            );

            Task<HttpResponse> __thiscall ForwardTokenRefreshRequest(
                _in const std::string& szRefreshToken,
                _in const std::string& szServerIdentifier
            );

        private:
            Task<HttpResponse> __thiscall SendAndReceive(
                _in int nBackendFileDescriptor,
                _in const Frame& sRequestFrame
            );

            Task<HttpResponse> __thiscall HandleLoginResponse(
                _in int nBackendFileDescriptor,
                _in const Frame& sRequestFrame,
                _in const EndpointDefinition& sEndpointDefinition
            );

            HttpResponse __thiscall BuildHttpResponseFromFrame(
                _in const Frame& sResponseFrame
            ) const;

            EventLoop&                      m_sEventLoop;
            UnixSocketListener&             m_sUnixSocketListener;
            TokenEngine&                    m_sTokenEngine;
            RequestFormatter                m_sRequestFormatter;
            FrameProtocol                   m_sFrameProtocol;
            std::unordered_map<int, std::string>    m_stdsznReadBuffers;
            std::mutex                      m_stdMutexReadBuffers;
    };

} // namespace Gateway
