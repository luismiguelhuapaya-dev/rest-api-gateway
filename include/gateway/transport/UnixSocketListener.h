#pragma once

#include "gateway/Common.h"
#include "gateway/core/EventLoop.h"
#include "gateway/core/Coroutine.h"
#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Gateway
{

    struct BackendConnection
    {
        int                 m_nFileDescriptor;
        std::string         m_szBackendIdentifier;
        ConnectionState     m_eState;
        std::string         m_szReadBuffer;
    };

    class UnixSocketListener
    {
        public:
            UnixSocketListener(
                _in EventLoop& sEventLoop
            );
            ~UnixSocketListener();

            UnixSocketListener(const UnixSocketListener&) = delete;
            UnixSocketListener& operator=(const UnixSocketListener&) = delete;

            bool __thiscall Bind(
                _in const std::string& szSocketPath
            );

            bool __thiscall Listen(
                _in uint32_t un32Backlog
            );

            Task<int> __thiscall AcceptConnection();

            void __thiscall Close();

            int __thiscall GetFileDescriptor() const;

            bool __thiscall IsListening() const;

            void __thiscall SetBackendConnectionCallback(
                _in std::function<Task<void>(int)> stdCallback
            );

            Task<void> __thiscall AcceptLoop();

            void __thiscall RegisterBackend(
                _in int nFileDescriptor,
                _in const std::string& szBackendIdentifier
            );

            void __thiscall UnregisterBackend(
                _in int nFileDescriptor
            );

            bool __thiscall HasBackend(
                _in const std::string& szBackendIdentifier
            ) const;

            int __thiscall GetBackendFileDescriptor(
                _in const std::string& szBackendIdentifier
            ) const;

            std::vector<std::string> __thiscall GetRegisteredBackendIdentifiers() const;

        private:
            bool __thiscall SetNonBlocking(
                _in int nFileDescriptor
            );

            EventLoop&                                      m_sEventLoop;
            int                                             m_nListenFileDescriptor;
            bool                                            m_bIsListening;
            std::string                                     m_szSocketPath;
            std::function<Task<void>(int)>                  m_stdBackendConnectionCallback;
            std::unordered_map<std::string, int>            m_stdsznBackendFileDescriptors;
            std::unordered_map<int, BackendConnection>      m_stdnsBackendConnections;
            static const uint32_t                           msc_un32DefaultBacklog = 32;
    };

} // namespace Gateway
