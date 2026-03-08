#pragma once

#include "gateway/Common.h"
#include "gateway/core/EventLoop.h"
#include "gateway/core/Coroutine.h"
#include <string>
#include <cstdint>
#include <vector>

namespace Gateway
{

    enum class FrameType
    {
        Registration,
        Unregistration,
        Request,
        Response,
        LoginResponse,
        TokenResponse,
        Heartbeat,
        Error
    };

    struct Frame
    {
        FrameType       m_eFrameType;
        uint32_t        m_un32PayloadLength;
        std::string     m_szPayload;
        uint32_t        m_un32RequestIdentifier;
    };

    class FrameProtocol
    {
        public:
            FrameProtocol();
            ~FrameProtocol() = default;

            bool __thiscall SerializeFrame(
                _in const Frame& sFrame,
                _out std::string& szSerializedData
            ) const;

            bool __thiscall DeserializeFrame(
                _in const std::string& szData,
                _out Frame& sFrame,
                _out uint32_t& un32BytesConsumed
            ) const;

            bool __thiscall HasCompleteFrame(
                _in const std::string& szBuffer
            ) const;

            Task<bool> __thiscall SendFrame(
                _in EventLoop& sEventLoop,
                _in int nFileDescriptor,
                _in const Frame& sFrame
            );

            Task<Frame> __thiscall ReceiveFrame(
                _in EventLoop& sEventLoop,
                _in int nFileDescriptor,
                _inout std::string& szReadBuffer
            );

            static const uint32_t       msc_un32HeaderSize = 12;
            static const uint32_t       msc_un32MaxPayloadSize = 1048576;

        private:
            void __thiscall WriteUint32BigEndian(
                _out uint8_t* pb8Output,
                _in uint32_t un32Value
            ) const;

            uint32_t __thiscall ReadUint32BigEndian(
                _in const uint8_t* pb8Input
            ) const;
    };

} // namespace Gateway
