#pragma once

#include "gateway/Common.h"
#include "gateway/core/Coroutine.h"
#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>

namespace Gateway
{

    enum class EventType
    {
        Read,
        Write,
        ReadWrite,
        Error,
        Hangup
    };

    struct EventData
    {
        int                             m_nFileDescriptor;
        EventType                       m_eEventType;
        std::coroutine_handle<>         m_stdCoroutineHandle;
    };

    class EpollAwaiter
    {
        public:
            EpollAwaiter(
                _in int nEpollFileDescriptor,
                _in int nTargetFileDescriptor,
                _in EventType eEventType
            );

            bool await_ready() const noexcept;

            void await_suspend(
                _in std::coroutine_handle<> stdCoroutineHandle
            );

            int await_resume() const noexcept;

        private:
            int             m_nEpollFileDescriptor;
            int             m_nTargetFileDescriptor;
            EventType       m_eEventType;
            int             m_nResult;
    };

    class EventLoop
    {
        public:
            EventLoop();
            ~EventLoop();

            EventLoop(const EventLoop&) = delete;
            EventLoop& operator=(const EventLoop&) = delete;

            bool __thiscall Initialize(
                _in uint32_t un32MaxEvents
            );

            void __thiscall Run();

            void __thiscall Stop();

            bool __thiscall AddFileDescriptor(
                _in int nFileDescriptor,
                _in EventType eEventType
            );

            bool __thiscall ModifyFileDescriptor(
                _in int nFileDescriptor,
                _in EventType eEventType
            );

            bool __thiscall RemoveFileDescriptor(
                _in int nFileDescriptor
            );

            void __thiscall ScheduleCoroutine(
                _in std::coroutine_handle<> stdCoroutineHandle
            );

            void __thiscall RegisterReadCallback(
                _in int nFileDescriptor,
                _in std::coroutine_handle<> stdCoroutineHandle
            );

            void __thiscall RegisterWriteCallback(
                _in int nFileDescriptor,
                _in std::coroutine_handle<> stdCoroutineHandle
            );

            int __thiscall GetEpollFileDescriptor() const;

            bool __thiscall IsRunning() const;

            EpollAwaiter __thiscall WaitForReadable(
                _in int nFileDescriptor
            );

            EpollAwaiter __thiscall WaitForWritable(
                _in int nFileDescriptor
            );

        private:
            void __thiscall ProcessPendingCoroutines();

            int                                                 m_nEpollFileDescriptor;
            std::atomic<bool>                                   m_stdab8Running;
            uint32_t                                            m_un32MaxEvents;
            std::unordered_map<int, std::coroutine_handle<>>    m_stdnstdReadCallbacks;
            std::unordered_map<int, std::coroutine_handle<>>    m_stdnstdWriteCallbacks;
            std::queue<std::coroutine_handle<>>                 m_stdstdPendingCoroutines;
            std::mutex                                          m_stdMutexCallbacks;
            std::mutex                                          m_stdMutexPendingCoroutines;
            static const uint32_t                               msc_un32DefaultMaxEvents = 1024;
            static const int                                    msc_nEpollTimeoutMilliseconds = 100;
    };

} // namespace Gateway
