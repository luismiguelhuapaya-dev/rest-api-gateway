#pragma once

#include "gateway/Common.h"
#include <coroutine>
#include <exception>
#include <optional>
#include <functional>
#include <variant>

namespace Gateway
{

    template<typename T>
    class Task
    {
        public:
            struct Promise;
            using promise_type = Promise;
            using HandleType = std::coroutine_handle<Promise>;

            struct Promise
            {
                std::variant<std::monostate, T, std::exception_ptr> m_stdValue;
                std::coroutine_handle<> m_stdCoroutineContinuation = nullptr;

                Task get_return_object()
                {
                    Task sTask;
                    sTask.m_stdCoroutineHandle = HandleType::from_promise(*this);
                    return sTask;
                }

                std::suspend_always initial_suspend() noexcept
                {
                    return {};
                }

                auto final_suspend() noexcept
                {
                    struct FinalAwaiter
                    {
                        bool await_ready() noexcept
                        {
                            return false;
                        }

                        std::coroutine_handle<> await_suspend(
                            _in HandleType stdCoroutineHandle
                        ) noexcept
                        {
                            std::coroutine_handle<> stdContinuation = stdCoroutineHandle.promise().m_stdCoroutineContinuation;
                            if (stdContinuation)
                            {
                                return stdContinuation;
                            }
                            return std::noop_coroutine();
                        }

                        void await_resume() noexcept
                        {
                        }
                    };
                    return FinalAwaiter{};
                }

                void return_value(
                    _in T sValue
                )
                {
                    m_stdValue = std::move(sValue);
                }

                void unhandled_exception()
                {
                    m_stdValue = std::current_exception();
                }
            };

            Task() : m_stdCoroutineHandle(nullptr)
            {
            }

            Task(
                _in Task&& sOther
            ) noexcept : m_stdCoroutineHandle(sOther.m_stdCoroutineHandle)
            {
                sOther.m_stdCoroutineHandle = nullptr;
            }

            Task& operator=(
                _in Task&& sOther
            ) noexcept
            {
                if (this != &sOther)
                {
                    if (m_stdCoroutineHandle)
                    {
                        m_stdCoroutineHandle.destroy();
                    }
                    m_stdCoroutineHandle = sOther.m_stdCoroutineHandle;
                    sOther.m_stdCoroutineHandle = nullptr;
                }
                return *this;
            }

            ~Task()
            {
                if (m_stdCoroutineHandle)
                {
                    m_stdCoroutineHandle.destroy();
                }
            }

            Task(const Task&) = delete;
            Task& operator=(const Task&) = delete;

            bool __thiscall IsReady() const
            {
                bool bResult = false;
                if ((m_stdCoroutineHandle) && (m_stdCoroutineHandle.done()))
                {
                    bResult = true;
                }
                return bResult;
            }

            void __thiscall Resume()
            {
                if ((m_stdCoroutineHandle) && (!m_stdCoroutineHandle.done()))
                {
                    m_stdCoroutineHandle.resume();
                }
            }

            T __thiscall GetResult()
            {
                T sResult{};
                auto& stdValue = m_stdCoroutineHandle.promise().m_stdValue;
                if (std::holds_alternative<std::exception_ptr>(stdValue))
                {
                    std::rethrow_exception(std::get<std::exception_ptr>(stdValue));
                }
                else if (std::holds_alternative<T>(stdValue))
                {
                    sResult = std::move(std::get<T>(stdValue));
                }
                return sResult;
            }

            auto operator co_await() const noexcept
            {
                struct TaskAwaiter
                {
                    HandleType m_stdCoroutineHandle;

                    bool await_ready() const noexcept
                    {
                        bool bResult = false;
                        if (m_stdCoroutineHandle.done())
                        {
                            bResult = true;
                        }
                        return bResult;
                    }

                    std::coroutine_handle<> await_suspend(
                        _in std::coroutine_handle<> stdCallerHandle
                    ) noexcept
                    {
                        m_stdCoroutineHandle.promise().m_stdCoroutineContinuation = stdCallerHandle;
                        return m_stdCoroutineHandle;
                    }

                    T await_resume()
                    {
                        T sResult{};
                        auto& stdValue = m_stdCoroutineHandle.promise().m_stdValue;
                        if (std::holds_alternative<std::exception_ptr>(stdValue))
                        {
                            std::rethrow_exception(std::get<std::exception_ptr>(stdValue));
                        }
                        else if (std::holds_alternative<T>(stdValue))
                        {
                            sResult = std::move(std::get<T>(stdValue));
                        }
                        return sResult;
                    }
                };
                return TaskAwaiter{m_stdCoroutineHandle};
            }

            HandleType __thiscall GetHandle() const
            {
                return m_stdCoroutineHandle;
            }

        private:
            HandleType m_stdCoroutineHandle;
    };

    template<>
    class Task<void>
    {
        public:
            struct Promise;
            using promise_type = Promise;
            using HandleType = std::coroutine_handle<Promise>;

            struct Promise
            {
                std::exception_ptr m_stdException = nullptr;
                std::coroutine_handle<> m_stdCoroutineContinuation = nullptr;

                Task get_return_object()
                {
                    Task sTask;
                    sTask.m_stdCoroutineHandle = HandleType::from_promise(*this);
                    return sTask;
                }

                std::suspend_always initial_suspend() noexcept
                {
                    return {};
                }

                auto final_suspend() noexcept
                {
                    struct FinalAwaiter
                    {
                        bool await_ready() noexcept
                        {
                            return false;
                        }

                        std::coroutine_handle<> await_suspend(
                            _in HandleType stdCoroutineHandle
                        ) noexcept
                        {
                            std::coroutine_handle<> stdContinuation = stdCoroutineHandle.promise().m_stdCoroutineContinuation;
                            if (stdContinuation)
                            {
                                return stdContinuation;
                            }
                            return std::noop_coroutine();
                        }

                        void await_resume() noexcept
                        {
                        }
                    };
                    return FinalAwaiter{};
                }

                void return_void()
                {
                }

                void unhandled_exception()
                {
                    m_stdException = std::current_exception();
                }
            };

            Task() : m_stdCoroutineHandle(nullptr)
            {
            }

            Task(
                _in Task&& sOther
            ) noexcept : m_stdCoroutineHandle(sOther.m_stdCoroutineHandle)
            {
                sOther.m_stdCoroutineHandle = nullptr;
            }

            Task& operator=(
                _in Task&& sOther
            ) noexcept
            {
                if (this != &sOther)
                {
                    if (m_stdCoroutineHandle)
                    {
                        m_stdCoroutineHandle.destroy();
                    }
                    m_stdCoroutineHandle = sOther.m_stdCoroutineHandle;
                    sOther.m_stdCoroutineHandle = nullptr;
                }
                return *this;
            }

            ~Task()
            {
                if (m_stdCoroutineHandle)
                {
                    m_stdCoroutineHandle.destroy();
                }
            }

            Task(const Task&) = delete;
            Task& operator=(const Task&) = delete;

            bool __thiscall IsReady() const
            {
                bool bResult = false;
                if ((m_stdCoroutineHandle) && (m_stdCoroutineHandle.done()))
                {
                    bResult = true;
                }
                return bResult;
            }

            void __thiscall Resume()
            {
                if ((m_stdCoroutineHandle) && (!m_stdCoroutineHandle.done()))
                {
                    m_stdCoroutineHandle.resume();
                }
            }

            void __thiscall GetResult()
            {
                if (m_stdCoroutineHandle.promise().m_stdException)
                {
                    std::rethrow_exception(m_stdCoroutineHandle.promise().m_stdException);
                }
            }

            auto operator co_await() const noexcept
            {
                struct TaskAwaiter
                {
                    HandleType m_stdCoroutineHandle;

                    bool await_ready() const noexcept
                    {
                        bool bResult = false;
                        if (m_stdCoroutineHandle.done())
                        {
                            bResult = true;
                        }
                        return bResult;
                    }

                    std::coroutine_handle<> await_suspend(
                        _in std::coroutine_handle<> stdCallerHandle
                    ) noexcept
                    {
                        m_stdCoroutineHandle.promise().m_stdCoroutineContinuation = stdCallerHandle;
                        return m_stdCoroutineHandle;
                    }

                    void await_resume()
                    {
                        if (m_stdCoroutineHandle.promise().m_stdException)
                        {
                            std::rethrow_exception(m_stdCoroutineHandle.promise().m_stdException);
                        }
                    }
                };
                return TaskAwaiter{m_stdCoroutineHandle};
            }

            HandleType __thiscall GetHandle() const
            {
                return m_stdCoroutineHandle;
            }

        private:
            HandleType m_stdCoroutineHandle;
    };

} // namespace Gateway
