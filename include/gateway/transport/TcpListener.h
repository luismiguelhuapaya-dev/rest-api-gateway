#pragma once

#include "gateway/Common.h"
#include "gateway/core/EventLoop.h"
#include "gateway/core/Coroutine.h"
#include <string>
#include <cstdint>
#include <functional>

namespace Gateway
{

    class TcpListener
    {
        public:
            TcpListener(
                _in EventLoop& sEventLoop
            );
            ~TcpListener();

            TcpListener(const TcpListener&) = delete;
            TcpListener& operator=(const TcpListener&) = delete;

            bool __thiscall Bind(
                _in const std::string& szAddress,
                _in uint16_t un16Port
            );

            bool __thiscall Listen(
                _in uint32_t un32Backlog
            );

            Task<int> __thiscall AcceptConnection();

            void __thiscall Close();

            int __thiscall GetFileDescriptor() const;

            bool __thiscall IsListening() const;

            Task<std::string> __thiscall ReadFromClient(
                _in int nClientFileDescriptor,
                _in uint32_t un32MaxBytes
            );

            Task<bool> __thiscall WriteToClient(
                _in int nClientFileDescriptor,
                _in const std::string& szData
            );

            void __thiscall SetConnectionCallback(
                _in std::function<Task<void>(int)> stdCallback
            );

            Task<void> __thiscall AcceptLoop();

        private:
            bool __thiscall SetNonBlocking(
                _in int nFileDescriptor
            );

            EventLoop&                              m_sEventLoop;
            int                                     m_nListenFileDescriptor;
            bool                                    m_bIsListening;
            std::function<Task<void>(int)>          m_stdConnectionCallback;
            static const uint32_t                   msc_un32DefaultBacklog = 128;
            static const uint32_t                   msc_un32ReadBufferSize = 65536;
    };

} // namespace Gateway
