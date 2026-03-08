#include "gateway/transport/FrameProtocol.h"
#include "gateway/logging/Logger.h"
#include <cstring>
#include <unistd.h>

namespace Gateway
{

    FrameProtocol::FrameProtocol()
    {
    }

    bool __thiscall FrameProtocol::SerializeFrame(
        _in const Frame& sFrame,
        _out std::string& szSerializedData
    ) const
    {
        bool bResult = false;

        if (sFrame.m_un32PayloadLength <= msc_un32MaxPayloadSize)
        {
            // Frame format: [4 bytes frame type][4 bytes request ID][4 bytes payload length][N bytes payload]
            szSerializedData.resize(msc_un32HeaderSize + sFrame.m_un32PayloadLength);

            uint8_t* pb8Output = reinterpret_cast<uint8_t*>(szSerializedData.data());

            WriteUint32BigEndian(pb8Output, static_cast<uint32_t>(sFrame.m_eFrameType));
            WriteUint32BigEndian(pb8Output + 4, sFrame.m_un32RequestIdentifier);
            WriteUint32BigEndian(pb8Output + 8, sFrame.m_un32PayloadLength);

            if (sFrame.m_un32PayloadLength > 0)
            {
                std::memcpy(pb8Output + msc_un32HeaderSize, sFrame.m_szPayload.data(), sFrame.m_un32PayloadLength);
            }

            bResult = true;
        }

        return bResult;
    }

    bool __thiscall FrameProtocol::DeserializeFrame(
        _in const std::string& szData,
        _out Frame& sFrame,
        _out uint32_t& un32BytesConsumed
    ) const
    {
        bool bResult = false;
        un32BytesConsumed = 0;

        if (szData.size() >= msc_un32HeaderSize)
        {
            const uint8_t* pb8Input = reinterpret_cast<const uint8_t*>(szData.data());

            uint32_t un32FrameType = ReadUint32BigEndian(pb8Input);
            uint32_t un32RequestIdentifier = ReadUint32BigEndian(pb8Input + 4);
            uint32_t un32PayloadLength = ReadUint32BigEndian(pb8Input + 8);

            if ((un32PayloadLength <= msc_un32MaxPayloadSize) &&
                (szData.size() >= (msc_un32HeaderSize + un32PayloadLength)))
            {
                sFrame.m_eFrameType = static_cast<FrameType>(un32FrameType);
                sFrame.m_un32RequestIdentifier = un32RequestIdentifier;
                sFrame.m_un32PayloadLength = un32PayloadLength;

                if (un32PayloadLength > 0)
                {
                    sFrame.m_szPayload.assign(
                        szData.data() + msc_un32HeaderSize,
                        un32PayloadLength
                    );
                }
                else
                {
                    sFrame.m_szPayload.clear();
                }

                un32BytesConsumed = msc_un32HeaderSize + un32PayloadLength;
                bResult = true;
            }
        }

        return bResult;
    }

    bool __thiscall FrameProtocol::HasCompleteFrame(
        _in const std::string& szBuffer
    ) const
    {
        bool bResult = false;

        if (szBuffer.size() >= msc_un32HeaderSize)
        {
            const uint8_t* pb8Input = reinterpret_cast<const uint8_t*>(szBuffer.data());
            uint32_t un32PayloadLength = ReadUint32BigEndian(pb8Input + 8);

            if ((un32PayloadLength <= msc_un32MaxPayloadSize) &&
                (szBuffer.size() >= (msc_un32HeaderSize + un32PayloadLength)))
            {
                bResult = true;
            }
        }

        return bResult;
    }

    Task<bool> __thiscall FrameProtocol::SendFrame(
        _in EventLoop& sEventLoop,
        _in int nFileDescriptor,
        _in const Frame& sFrame
    )
    {
        bool bResult = false;

        std::string szSerializedData;
        if (SerializeFrame(sFrame, szSerializedData))
        {
            size_t un64TotalWritten = 0;
            size_t un64DataSize = szSerializedData.size();

            while (un64TotalWritten < un64DataSize)
            {
                co_await sEventLoop.WaitForWritable(nFileDescriptor);

                ssize_t nBytesWritten = write(
                    nFileDescriptor,
                    szSerializedData.data() + un64TotalWritten,
                    un64DataSize - un64TotalWritten
                );

                if (nBytesWritten > 0)
                {
                    un64TotalWritten += static_cast<size_t>(nBytesWritten);
                }
                else if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    GATEWAY_LOG_ERROR("FrameProtocol", "Write error on fd " + std::to_string(nFileDescriptor));
                    break;
                }
            }

            if (un64TotalWritten == un64DataSize)
            {
                bResult = true;
            }
        }

        co_return bResult;
    }

    Task<Frame> __thiscall FrameProtocol::ReceiveFrame(
        _in EventLoop& sEventLoop,
        _in int nFileDescriptor,
        _inout std::string& szReadBuffer
    )
    {
        Frame sFrame;
        sFrame.m_eFrameType = FrameType::Error;
        sFrame.m_un32PayloadLength = 0;
        sFrame.m_un32RequestIdentifier = 0;

        while (!HasCompleteFrame(szReadBuffer))
        {
            co_await sEventLoop.WaitForReadable(nFileDescriptor);

            char achBuffer[4096];
            ssize_t nBytesRead = read(nFileDescriptor, achBuffer, sizeof(achBuffer));

            if (nBytesRead > 0)
            {
                szReadBuffer.append(achBuffer, static_cast<size_t>(nBytesRead));
            }
            else if (nBytesRead == 0)
            {
                // Connection closed
                break;
            }
            else if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                GATEWAY_LOG_ERROR("FrameProtocol", "Read error on fd " + std::to_string(nFileDescriptor));
                break;
            }
        }

        if (HasCompleteFrame(szReadBuffer))
        {
            uint32_t un32BytesConsumed = 0;
            if (DeserializeFrame(szReadBuffer, sFrame, un32BytesConsumed))
            {
                szReadBuffer.erase(0, un32BytesConsumed);
            }
        }

        co_return sFrame;
    }

    void __thiscall FrameProtocol::WriteUint32BigEndian(
        _out uint8_t* pb8Output,
        _in uint32_t un32Value
    ) const
    {
        pb8Output[0] = static_cast<uint8_t>((un32Value >> 24) & 0xFF);
        pb8Output[1] = static_cast<uint8_t>((un32Value >> 16) & 0xFF);
        pb8Output[2] = static_cast<uint8_t>((un32Value >> 8) & 0xFF);
        pb8Output[3] = static_cast<uint8_t>(un32Value & 0xFF);
    }

    uint32_t __thiscall FrameProtocol::ReadUint32BigEndian(
        _in const uint8_t* pb8Input
    ) const
    {
        uint32_t un32Result =
            (static_cast<uint32_t>(pb8Input[0]) << 24) |
            (static_cast<uint32_t>(pb8Input[1]) << 16) |
            (static_cast<uint32_t>(pb8Input[2]) << 8) |
            (static_cast<uint32_t>(pb8Input[3]));

        return un32Result;
    }

} // namespace Gateway
