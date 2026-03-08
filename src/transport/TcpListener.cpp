#include "gateway/transport/TcpListener.h"
#include "gateway/logging/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace Gateway
{

    TcpListener::TcpListener(
        _in EventLoop& sEventLoop
    )
        : m_sEventLoop(sEventLoop)
        , m_nListenFileDescriptor(-1)
        , m_bIsListening(false)
    {
    }

    TcpListener::~TcpListener()
    {
        Close();
    }

    bool __thiscall TcpListener::Bind(
        _in const std::string& szAddress,
        _in uint16_t un16Port
    )
    {
        bool bResult = false;

        m_nListenFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
        if (m_nListenFileDescriptor >= 0)
        {
            int nReuseAddress = 1;
            setsockopt(m_nListenFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &nReuseAddress, sizeof(nReuseAddress));

            struct sockaddr_in sAddress{};
            sAddress.sin_family = AF_INET;
            sAddress.sin_port = htons(un16Port);

            if (inet_pton(AF_INET, szAddress.c_str(), &sAddress.sin_addr) > 0)
            {
                if (bind(m_nListenFileDescriptor, reinterpret_cast<struct sockaddr*>(&sAddress), sizeof(sAddress)) == 0)
                {
                    if (SetNonBlocking(m_nListenFileDescriptor))
                    {
                        bResult = true;
                        GATEWAY_LOG_INFO("TcpListener", "Bound to " + szAddress + ":" + std::to_string(un16Port));
                    }
                }
                else
                {
                    GATEWAY_LOG_ERROR("TcpListener", "Failed to bind: " + std::string(strerror(errno)));
                    close(m_nListenFileDescriptor);
                    m_nListenFileDescriptor = -1;
                }
            }
            else
            {
                GATEWAY_LOG_ERROR("TcpListener", "Invalid address: " + szAddress);
                close(m_nListenFileDescriptor);
                m_nListenFileDescriptor = -1;
            }
        }
        else
        {
            GATEWAY_LOG_ERROR("TcpListener", "Failed to create socket: " + std::string(strerror(errno)));
        }

        return bResult;
    }

    bool __thiscall TcpListener::Listen(
        _in uint32_t un32Backlog
    )
    {
        bool bResult = false;

        if (m_nListenFileDescriptor >= 0)
        {
            if (listen(m_nListenFileDescriptor, static_cast<int>(un32Backlog)) == 0)
            {
                m_bIsListening = true;
                m_sEventLoop.AddFileDescriptor(m_nListenFileDescriptor, EventType::Read);
                bResult = true;
                GATEWAY_LOG_INFO("TcpListener", "Listening with backlog " + std::to_string(un32Backlog));
            }
            else
            {
                GATEWAY_LOG_ERROR("TcpListener", "Failed to listen: " + std::string(strerror(errno)));
            }
        }

        return bResult;
    }

    Task<int> __thiscall TcpListener::AcceptConnection()
    {
        int nClientFileDescriptor = -1;

        while (nClientFileDescriptor < 0)
        {
            co_await m_sEventLoop.WaitForReadable(m_nListenFileDescriptor);

            struct sockaddr_in sClientAddress{};
            socklen_t un32AddressLength = sizeof(sClientAddress);
            nClientFileDescriptor = accept(
                m_nListenFileDescriptor,
                reinterpret_cast<struct sockaddr*>(&sClientAddress),
                &un32AddressLength
            );

            if (nClientFileDescriptor >= 0)
            {
                SetNonBlocking(nClientFileDescriptor);
                char aszClientIp[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sClientAddress.sin_addr, aszClientIp, INET_ADDRSTRLEN);
                GATEWAY_LOG_DEBUG("TcpListener", "Accepted connection from " + std::string(aszClientIp));
            }
            else if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                GATEWAY_LOG_ERROR("TcpListener", "Accept failed: " + std::string(strerror(errno)));
            }
            else
            {
                nClientFileDescriptor = -1;
            }
        }

        co_return nClientFileDescriptor;
    }

    void __thiscall TcpListener::Close()
    {
        if (m_nListenFileDescriptor >= 0)
        {
            m_sEventLoop.RemoveFileDescriptor(m_nListenFileDescriptor);
            close(m_nListenFileDescriptor);
            m_nListenFileDescriptor = -1;
            m_bIsListening = false;
            GATEWAY_LOG_INFO("TcpListener", "Listener closed");
        }
    }

    int __thiscall TcpListener::GetFileDescriptor() const
    {
        return m_nListenFileDescriptor;
    }

    bool __thiscall TcpListener::IsListening() const
    {
        return m_bIsListening;
    }

    Task<std::string> __thiscall TcpListener::ReadFromClient(
        _in int nClientFileDescriptor,
        _in uint32_t un32MaxBytes
    )
    {
        std::string szResult;
        std::vector<char> stdvchBuffer(un32MaxBytes);

        co_await m_sEventLoop.WaitForReadable(nClientFileDescriptor);

        ssize_t nBytesRead = read(nClientFileDescriptor, stdvchBuffer.data(), stdvchBuffer.size());
        if (nBytesRead > 0)
        {
            szResult.assign(stdvchBuffer.data(), static_cast<size_t>(nBytesRead));
        }
        else if (nBytesRead == 0)
        {
            // Connection closed
            szResult = "";
        }
        else
        {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                GATEWAY_LOG_ERROR("TcpListener", "Read error: " + std::string(strerror(errno)));
            }
        }

        co_return szResult;
    }

    Task<bool> __thiscall TcpListener::WriteToClient(
        _in int nClientFileDescriptor,
        _in const std::string& szData
    )
    {
        bool bResult = false;
        size_t un64TotalWritten = 0;
        size_t un64DataSize = szData.size();

        while (un64TotalWritten < un64DataSize)
        {
            co_await m_sEventLoop.WaitForWritable(nClientFileDescriptor);

            ssize_t nBytesWritten = write(
                nClientFileDescriptor,
                szData.data() + un64TotalWritten,
                un64DataSize - un64TotalWritten
            );

            if (nBytesWritten > 0)
            {
                un64TotalWritten += static_cast<size_t>(nBytesWritten);
            }
            else if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                GATEWAY_LOG_ERROR("TcpListener", "Write error: " + std::string(strerror(errno)));
                break;
            }
        }

        if (un64TotalWritten == un64DataSize)
        {
            bResult = true;
        }

        co_return bResult;
    }

    void __thiscall TcpListener::SetConnectionCallback(
        _in std::function<Task<void>(int)> stdCallback
    )
    {
        m_stdConnectionCallback = std::move(stdCallback);
    }

    Task<void> __thiscall TcpListener::AcceptLoop()
    {
        while (m_bIsListening)
        {
            int nClientFileDescriptor = co_await AcceptConnection();
            if ((nClientFileDescriptor >= 0) && (m_stdConnectionCallback))
            {
                auto stdTask = m_stdConnectionCallback(nClientFileDescriptor);
                m_sEventLoop.ScheduleCoroutine(stdTask.GetHandle());
            }
        }
    }

    bool __thiscall TcpListener::SetNonBlocking(
        _in int nFileDescriptor
    )
    {
        bool bResult = false;

        int nFlags = fcntl(nFileDescriptor, F_GETFL, 0);
        if (nFlags >= 0)
        {
            if (fcntl(nFileDescriptor, F_SETFL, nFlags | O_NONBLOCK) == 0)
            {
                bResult = true;
            }
        }

        return bResult;
    }

} // namespace Gateway
