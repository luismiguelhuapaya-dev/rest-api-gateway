#include "gateway/core/EventLoop.h"
#include "gateway/logging/Logger.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace Gateway
{

    // EpollAwaiter implementation
    EpollAwaiter::EpollAwaiter(
        _in int nEpollFileDescriptor,
        _in int nTargetFileDescriptor,
        _in EventType eEventType
    )
        : m_nEpollFileDescriptor(nEpollFileDescriptor)
        , m_nTargetFileDescriptor(nTargetFileDescriptor)
        , m_eEventType(eEventType)
        , m_nResult(0)
    {
    }

    bool EpollAwaiter::await_ready() const noexcept
    {
        return false;
    }

    void EpollAwaiter::await_suspend(
        _in std::coroutine_handle<> stdCoroutineHandle
    )
    {
        struct epoll_event sEvent{};
        sEvent.data.ptr = stdCoroutineHandle.address();

        if (m_eEventType == EventType::Read)
        {
            sEvent.events = EPOLLIN | EPOLLONESHOT;
        }
        else if (m_eEventType == EventType::Write)
        {
            sEvent.events = EPOLLOUT | EPOLLONESHOT;
        }
        else if (m_eEventType == EventType::ReadWrite)
        {
            sEvent.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
        }

        int nResult = epoll_ctl(m_nEpollFileDescriptor, EPOLL_CTL_MOD, m_nTargetFileDescriptor, &sEvent);
        if (nResult < 0)
        {
            nResult = epoll_ctl(m_nEpollFileDescriptor, EPOLL_CTL_ADD, m_nTargetFileDescriptor, &sEvent);
        }
    }

    int EpollAwaiter::await_resume() const noexcept
    {
        return m_nResult;
    }

    // EventLoop implementation
    EventLoop::EventLoop()
        : m_nEpollFileDescriptor(-1)
        , m_stdab8Running(false)
        , m_un32MaxEvents(msc_un32DefaultMaxEvents)
    {
    }

    EventLoop::~EventLoop()
    {
        Stop();
        if (m_nEpollFileDescriptor >= 0)
        {
            close(m_nEpollFileDescriptor);
            m_nEpollFileDescriptor = -1;
        }
    }

    bool __thiscall EventLoop::Initialize(
        _in uint32_t un32MaxEvents
    )
    {
        bool bResult = false;

        m_un32MaxEvents = un32MaxEvents;
        m_nEpollFileDescriptor = epoll_create1(0);

        if (m_nEpollFileDescriptor >= 0)
        {
            bResult = true;
            GATEWAY_LOG_INFO("EventLoop", "Event loop initialized successfully");
        }
        else
        {
            GATEWAY_LOG_ERROR("EventLoop", "Failed to create epoll file descriptor");
        }

        return bResult;
    }

    void __thiscall EventLoop::Run()
    {
        m_stdab8Running.store(true);

        std::vector<struct epoll_event> stdsEvents(m_un32MaxEvents);

        while (m_stdab8Running.load())
        {
            ProcessPendingCoroutines();

            int nEventCount = epoll_wait(
                m_nEpollFileDescriptor,
                stdsEvents.data(),
                static_cast<int>(m_un32MaxEvents),
                msc_nEpollTimeoutMilliseconds
            );

            if (nEventCount < 0)
            {
                if (errno != EINTR)
                {
                    GATEWAY_LOG_ERROR("EventLoop", "epoll_wait failed: " + std::string(strerror(errno)));
                }
            }
            else
            {
                for (int nIndex = 0; (nIndex < nEventCount); ++nIndex)
                {
                    void* pData = stdsEvents[nIndex].data.ptr;
                    if (pData != nullptr)
                    {
                        auto stdCoroutineHandle = std::coroutine_handle<>::from_address(pData);
                        if ((!stdCoroutineHandle.done()))
                        {
                            stdCoroutineHandle.resume();
                        }
                    }
                }
            }
        }
    }

    void __thiscall EventLoop::Stop()
    {
        m_stdab8Running.store(false);
        GATEWAY_LOG_INFO("EventLoop", "Event loop stopping");
    }

    bool __thiscall EventLoop::AddFileDescriptor(
        _in int nFileDescriptor,
        _in EventType eEventType
    )
    {
        bool bResult = false;

        struct epoll_event sEvent{};
        sEvent.data.fd = nFileDescriptor;

        if (eEventType == EventType::Read)
        {
            sEvent.events = EPOLLIN;
        }
        else if (eEventType == EventType::Write)
        {
            sEvent.events = EPOLLOUT;
        }
        else if (eEventType == EventType::ReadWrite)
        {
            sEvent.events = EPOLLIN | EPOLLOUT;
        }

        if (epoll_ctl(m_nEpollFileDescriptor, EPOLL_CTL_ADD, nFileDescriptor, &sEvent) == 0)
        {
            bResult = true;
        }

        return bResult;
    }

    bool __thiscall EventLoop::ModifyFileDescriptor(
        _in int nFileDescriptor,
        _in EventType eEventType
    )
    {
        bool bResult = false;

        struct epoll_event sEvent{};
        sEvent.data.fd = nFileDescriptor;

        if (eEventType == EventType::Read)
        {
            sEvent.events = EPOLLIN;
        }
        else if (eEventType == EventType::Write)
        {
            sEvent.events = EPOLLOUT;
        }
        else if (eEventType == EventType::ReadWrite)
        {
            sEvent.events = EPOLLIN | EPOLLOUT;
        }

        if (epoll_ctl(m_nEpollFileDescriptor, EPOLL_CTL_MOD, nFileDescriptor, &sEvent) == 0)
        {
            bResult = true;
        }

        return bResult;
    }

    bool __thiscall EventLoop::RemoveFileDescriptor(
        _in int nFileDescriptor
    )
    {
        bool bResult = false;

        if (epoll_ctl(m_nEpollFileDescriptor, EPOLL_CTL_DEL, nFileDescriptor, nullptr) == 0)
        {
            bResult = true;
        }

        // Remove any pending callbacks
        {
            std::lock_guard<std::mutex> stdLock(m_stdMutexCallbacks);
            m_stdnstdReadCallbacks.erase(nFileDescriptor);
            m_stdnstdWriteCallbacks.erase(nFileDescriptor);
        }

        return bResult;
    }

    void __thiscall EventLoop::ScheduleCoroutine(
        _in std::coroutine_handle<> stdCoroutineHandle
    )
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexPendingCoroutines);
        m_stdstdPendingCoroutines.push(stdCoroutineHandle);
    }

    void __thiscall EventLoop::RegisterReadCallback(
        _in int nFileDescriptor,
        _in std::coroutine_handle<> stdCoroutineHandle
    )
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexCallbacks);
        m_stdnstdReadCallbacks[nFileDescriptor] = stdCoroutineHandle;
    }

    void __thiscall EventLoop::RegisterWriteCallback(
        _in int nFileDescriptor,
        _in std::coroutine_handle<> stdCoroutineHandle
    )
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexCallbacks);
        m_stdnstdWriteCallbacks[nFileDescriptor] = stdCoroutineHandle;
    }

    int __thiscall EventLoop::GetEpollFileDescriptor() const
    {
        return m_nEpollFileDescriptor;
    }

    bool __thiscall EventLoop::IsRunning() const
    {
        return m_stdab8Running.load();
    }

    EpollAwaiter __thiscall EventLoop::WaitForReadable(
        _in int nFileDescriptor
    )
    {
        return EpollAwaiter(m_nEpollFileDescriptor, nFileDescriptor, EventType::Read);
    }

    EpollAwaiter __thiscall EventLoop::WaitForWritable(
        _in int nFileDescriptor
    )
    {
        return EpollAwaiter(m_nEpollFileDescriptor, nFileDescriptor, EventType::Write);
    }

    void __thiscall EventLoop::ProcessPendingCoroutines()
    {
        std::queue<std::coroutine_handle<>> stdLocalQueue;

        {
            std::lock_guard<std::mutex> stdLock(m_stdMutexPendingCoroutines);
            std::swap(stdLocalQueue, m_stdstdPendingCoroutines);
        }

        while (!stdLocalQueue.empty())
        {
            auto stdCoroutineHandle = stdLocalQueue.front();
            stdLocalQueue.pop();

            if ((!stdCoroutineHandle.done()))
            {
                stdCoroutineHandle.resume();
            }
        }
    }

} // namespace Gateway
