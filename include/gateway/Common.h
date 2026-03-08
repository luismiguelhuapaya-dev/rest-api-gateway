#pragma once

#define _in
#define _out
#define _inout
#define __stdcall
#define __thiscall
#define __fastcall

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <variant>
#include <chrono>
#include <string_view>
#include <array>
#include <algorithm>
#include <sstream>
#include <coroutine>

namespace Gateway
{
    enum class ResultCode
    {
        Success,
        Failure,
        Timeout,
        InvalidParameter,
        NotFound,
        Unauthorized,
        InternalError,
        ConnectionRefused,
        ParseError,
        ValidationError
    };

    enum class HttpMethod
    {
        Get,
        Post,
        Put,
        Delete,
        Patch,
        Head,
        Options,
        Unknown
    };

    enum class TokenType
    {
        Access,
        Refresh
    };

    enum class ParameterType
    {
        String,
        Integer,
        Float,
        Boolean,
        Object,
        Array
    };

    enum class ParameterLocation
    {
        Path,
        Query,
        Header,
        Body
    };

    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    enum class ConnectionState
    {
        Idle,
        Reading,
        Writing,
        Closing,
        Closed
    };

    HttpMethod __stdcall StringToHttpMethod(
        _in const std::string& szMethod
    );

    std::string __stdcall HttpMethodToString(
        _in HttpMethod eMethod
    );

} // namespace Gateway
