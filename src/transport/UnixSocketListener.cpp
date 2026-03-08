#include "gateway/transport/UnixSocketListener.h"
#include "gateway/logging/Logger.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace Gateway
{

    UnixSocketListener::UnixSocketListener(
        _in EventLoop& sEventLoop
    )
        : m_sEventLoop(sEventLoop)
        , m_nListenFileDescriptor(-1)
        , m_bIsListening(false)
    {
    }

    UnixSocketListener::~UnixSocketListener()
    {
        Close();
    }

    bool __thiscall UnixSocketListener::Bind(
        _in const std::string& szSocketPath
    )
    {
        bool bResult = false;

        m_szSocketPath = szSocketPath;

        // Remove existing socket file if present
        unlink(szSocketPath.c_str());

        m_nListenFileDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_nListenFileDescriptor >= 0)
        {
            struct sockaddr_un sAddress{};
            sAddress.sun_family = AF_UNIX;
            strncpy(sAddress.sun_path, szSocketPath.c_str(), sizeof(sAddress.sun_path) - 1);

            if (bind(m_nListenFileDescriptor, reinterpret_cast<struct sockaddr*>(&sAddress), sizeof(sAddress)) == 0)
            {
                if (SetNonBlocking(m_nListenFileDescriptor))
                {
                    bResult = true;
                    GATEWAY_LOG_INFO("UnixSocketListener", "Bound to " + szSocketPath);
                }
            }
            else
            {
                GATEWAY_LOG_ERROR("UnixSocketListener", "Failed to bind: " + std::string(strerror(errno)));
                close(m_nListenFileDescriptor);
                m_nListenFileDescriptor = -1;
            }
        }
        else
        {
            GATEWAY_LOG_ERROR("UnixSocketListener", "Failed to create socket: " + std::string(strerror(errno)));
        }

        return bResult;
    }

    bool __thiscall UnixSocketListener::Listen(
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
                GATEWAY_LOG_INFO("UnixSocketListener", "Listening on Unix domain socket");
            }
            else
            {
                GATEWAY_LOG_ERROR("UnixSocketListener", "Failed to listen: " + std::string(strerror(errno)));
            }
        }

        return bResult;
    }

    Task<int> __thiscall UnixSocketListener::AcceptConnection()
    {
        int nClientFileDescriptor = -1;

        while (nClientFileDescriptor < 0)
        {
            co_await m_sEventLoop.WaitForReadable(m_nListenFileDescriptor);

            struct sockaddr_un sClientAddress{};
            socklen_t un32AddressLength = sizeof(sClientAddress);
            nClientFileDescriptor = accept(
                m_nListenFileDescriptor,
                reinterpret_cast<struct sockaddr*>(&sClientAddress),
                &un32AddressLength
            );

            if (nClientFileDescriptor >= 0)
            {
                SetNonBlocking(nClientFileDescriptor);
                GATEWAY_LOG_DEBUG("UnixSocketListener", "Accepted backend connection on fd " + std::to_string(nClientFileDescriptor));
            }
            else if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                GATEWAY_LOG_ERROR("UnixSocketListener", "Accept failed: " + std::string(strerror(errno)));
            }
            else
            {
                nClientFileDescriptor = -1;
            }
        }

        co_return nClientFileDescriptor;
    }

    void __thiscall UnixSocketListener::Close()
    {
        if (m_nListenFileDescriptor >= 0)
        {
            m_sEventLoop.RemoveFileDescriptor(m_nListenFileDescriptor);
            close(m_nListenFileDescriptor);
            m_nListenFileDescriptor = -1;
            m_bIsListening = false;
            unlink(m_szSocketPath.c_str());
            GATEWAY_LOG_INFO("UnixSocketListener", "Unix socket listener closed");
        }

        // Close all backend connections
        for (auto& stdPair : m_stdnsBackendConnections)
        {
            close(stdPair.first);
        }
        m_stdnsBackendConnections.clear();
        m_stdsznBackendFileDescriptors.clear();
    }

    int __thiscall UnixSocketListener::GetFileDescriptor() const
    {
        return m_nListenFileDescriptor;
    }

    bool __thiscall UnixSocketListener::IsListening() const
    {
        return m_bIsListening;
    }

    void __thiscall UnixSocketListener::SetBackendConnectionCallback(
        _in std::function<Task<void>(int)> stdCallback
    )
    {
        m_stdBackendConnectionCallback = std::move(stdCallback);
    }

    Task<void> __thiscall UnixSocketListener::AcceptLoop()
    {
        while (m_bIsListening)
        {
            int nBackendFileDescriptor = co_await AcceptConnection();
            if ((nBackendFileDescriptor >= 0) && (m_stdBackendConnectionCallback))
            {
                auto stdTask = m_stdBackendConnectionCallback(nBackendFileDescriptor);
                m_sEventLoop.ScheduleCoroutine(stdTask.GetHandle());
            }
        }
    }

    void __thiscall UnixSocketListener::RegisterBackend(
        _in int nFileDescriptor,
        _in const std::string& szBackendIdentifier
    )
    {
        BackendConnection sConnection;
        sConnection.m_nFileDescriptor = nFileDescriptor;
        sConnection.m_szBackendIdentifier = szBackendIdentifier;
        sConnection.m_eState = ConnectionState::Idle;

        m_stdnsBackendConnections[nFileDescriptor] = sConnection;
        m_stdsznBackendFileDescriptors[szBackendIdentifier] = nFileDescriptor;

        GATEWAY_LOG_INFO("UnixSocketListener", "Registered backend: " + szBackendIdentifier + " on fd " + std::to_string(nFileDescriptor));
    }

    void __thiscall UnixSocketListener::UnregisterBackend(
        _in int nFileDescriptor
    )
    {
        auto stdIterator = m_stdnsBackendConnections.find(nFileDescriptor);
        if (stdIterator != m_stdnsBackendConnections.end())
        {
            std::string szBackendIdentifier = stdIterator->second.m_szBackendIdentifier;
            m_stdsznBackendFileDescriptors.erase(szBackendIdentifier);
            m_stdnsBackendConnections.erase(stdIterator);
            close(nFileDescriptor);
            GATEWAY_LOG_INFO("UnixSocketListener", "Unregistered backend: " + szBackendIdentifier);
        }
    }

    bool __thiscall UnixSocketListener::HasBackend(
        _in const std::string& szBackendIdentifier
    ) const
    {
        bool bResult = false;

        if (m_stdsznBackendFileDescriptors.find(szBackendIdentifier) != m_stdsznBackendFileDescriptors.end())
        {
            bResult = true;
        }

        return bResult;
    }

    int __thiscall UnixSocketListener::GetBackendFileDescriptor(
        _in const std::string& szBackendIdentifier
    ) const
    {
        int nResult = -1;

        auto stdIterator = m_stdsznBackendFileDescriptors.find(szBackendIdentifier);
        if (stdIterator != m_stdsznBackendFileDescriptors.end())
        {
            nResult = stdIterator->second;
        }

        return nResult;
    }

    std::vector<std::string> __thiscall UnixSocketListener::GetRegisteredBackendIdentifiers() const
    {
        std::vector<std::string> stdszResult;
        stdszResult.reserve(m_stdsznBackendFileDescriptors.size());

        for (const auto& stdPair : m_stdsznBackendFileDescriptors)
        {
            stdszResult.push_back(stdPair.first);
        }

        return stdszResult;
    }

    bool __thiscall UnixSocketListener::SetNonBlocking(
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
