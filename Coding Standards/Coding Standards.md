# C++20 Coding Standards — Dynamic REST API Gateway Server

_All contributors must follow these standards. Code reviews will enforce compliance._

---

## 1. Hungarian Notation

All variables use Hungarian notation. The prefix encodes scope, mutability, and data type, followed by a descriptive name in PascalCase.

### 1.1 Scope and Mutability Prefixes

The following prefixes appear before the underscore separator. They may be combined. Class member variables always use the m_ prefix.

| Prefix | Meaning | Example |
|--------|---------|---------|
| g_ | Global variable | g_unConnectionCount |
| s_ | Static variable (file/function scope) | s_unInstanceCount |
| c_ | Constant (local scope) | c_unMaxRetries |
| m_ | Class/struct member variable | m_szServerId |
| gc_ | Global constant | gc_szApplicationName |
| gs_ | Global static variable | gs_unStartupTime |
| gsc_ | Global static constant | gsc_szVersionString |
| sc_ | Static constant (file/function scope) | sc_un32BufferSize |
| ms_ | Member static variable | ms_un32InstanceCount |
| mc_ | Member constant (const member) | mc_szEndpointPath |
| msc_ | Member static constant | msc_un32MaxPayloadSize |
| (none) | Local variable | unIndex, szName |

The m_ prefix is mandatory for all class and struct member variables. Use ms_ for static members, mc_ for const members, and msc_ for static const members. This makes it immediately obvious whether a variable is local, a regular member, or a static/const member.

{code:cpp}
class ConnectionManager
{
    private:
        uint32_t                    m_un32MaxConnections;
        std::string                 m_szListenAddress;
        bool                        m_bIsRunning;
        const std::string           mc_szDefaultAddress;
        static uint32_t             ms_un32TotalInstances;
        static const uint32_t       msc_un32MaxInstances = 64;
};
{code}

### 1.2 Data Type Prefixes

The type prefix immediately precedes the PascalCase name (no underscore between type and name). For scoped variables, the scope prefix + underscore comes first, then the type prefix + name.

| Prefix | Type | C++ Type | Example |
|--------|------|----------|---------|
| n | Signed integer (platform width) | int | nCount |
| n16 | 16-bit signed integer | int16_t | n16Port |
| n32 | 32-bit signed integer | int32_t | n32Offset |
| n64 | 64-bit signed integer | int64_t | n64Timestamp |
| u | Unsigned integer (platform width) | unsigned int | uFlags |
| un | Unsigned integer (with name) | unsigned int | unIndex |
| un16 | 16-bit unsigned integer | uint16_t | un16Port |
| un32 | 32-bit unsigned integer | uint32_t | un32Address |
| un64 | 64-bit unsigned integer | uint64_t | un64Size |
| b8 | 8-bit byte / unsigned char | uint8_t | b8Value |
| b | Boolean | bool | bIsValid |
| ch | Character | char | chDelimiter |
| sz | String | std::string | szName |
| fl32 | 32-bit float | float | fl32Weight |
| fl64 | 64-bit float / double | double | fl64Elapsed |
| e | Enum value | enum class | eTokenType |
| s | Struct or class instance | SomeClass | sConfig |
| p | Pointer (raw) | T* | pszBuffer |
| pp | Pointer to pointer | T** | ppb8Data |
| a | Array (C-style) | T[] | ab8Buffer |
| fn | Function pointer / callback | void(*)() | fnCallback |

### 1.3 STL Type Prefix

All C++ Standard Library types use the single prefix std. The STL has too many container and utility types to maintain individual prefixes for each one. The std prefix signals that the variable is an STL construct; the actual C++ type declaration provides the specifics.

The std prefix is followed by the element type prefix (where applicable) and then the PascalCase name.

{code:cpp}
std::vector<std::string>                stdszEndpoints;
std::unordered_map<std::string, int>    stdsznConnections;
std::optional<std::string>              stdszUsername;
std::string_view                        stdsvHeaderValue;
std::shared_ptr<Connection>             stdsConnection;
std::unique_ptr<Socket>                 stdsSocket;
std::atomic<uint32_t>                   stdun32Count;
std::mutex                              stdMutexRoutes;
std::function<void()>                   stdCallback;
std::variant<int, std::string>          stdValue;
std::pair<std::string, int>             stdKeyValue;
{code}

Note: sz remains the preferred shorthand for std::string in most contexts because it is by far the most common type. Use std only for STL types other than std::string (vectors, maps, optionals, smart pointers, atomics, etc.). This avoids double-prefixing the most frequently used type in the codebase.

With scope prefixes:

{code:cpp}
m_stdszEndpoints              // Member: vector of strings
m_stdsznConnections           // Member: unordered_map<string, int>
ms_stdun32ActiveConnections   // Member static: atomic uint32
msc_un32MaxPayloadSize        // Member static constant: uint32
gsc_szVersionString           // Global static constant: string
g_stdun32TotalRequests        // Global: atomic uint32
s_stdMutexRoutingTable        // Static: mutex guarding the routing table
stdsEndpoints                 // Local: vector of struct/class instances
stdsSocket                    // Local: unique_ptr to a struct/class
stdsvHeaderValue              // Local: string_view
stdszUsername                 // Local: optional<string>
{code}

---

## 2. Naming Conventions

### 2.1 Variables

All variable names must be complete, descriptive English words in PascalCase. Single-letter or abbreviated names are not permitted. Every word in the name begins with an uppercase letter.

{code:cpp}
// CORRECT
uint32_t un32ConnectionIndex = 0;
std::string szServerIdentifier = "myplatform";
bool bIsAuthenticated = false;
std::vector<std::string> stdszRegisteredPaths;

// WRONG - single letters, abbreviations, lowercase
uint32_t i = 0;         // Must be a descriptive word
std::string srvId;      // Abbreviated; use szServerIdentifier
bool auth;              // Abbreviated; use bIsAuthenticated
{code}

### 2.2 Functions and Methods

Function and method names follow the same rules: complete descriptive words, PascalCase, no abbreviations.

{code:cpp}
// CORRECT
bool ValidateRequestParameters(
    _in const Request& sRequest
) const;

void RegisterEndpointDictionary(
    _in const Dictionary& sDictionary
);

Token GenerateAccessToken(
    _in const std::string& szServerId
);

// WRONG
bool ValReqParams();    // Abbreviated
void regEpDict();       // Abbreviated and unclear
{code}

### 2.3 Classes and Structs

Class and struct names are PascalCase with complete words. No C-style prefixes (no "C" or "S" prefix on class names).

{code:cpp}
// CORRECT
class RoutingTable { };
struct EndpointDefinition { };
class ValidationEngine { };

// WRONG
class CRouteTbl { };     // Prefixed, abbreviated
struct ep_def { };       // Lowercase, abbreviated
{code}

### 2.4 Enums

All enums must use enum class (scoped enums). The enum type name is PascalCase. Enum values are PascalCase. Variables holding enum values use the e prefix.

{code:cpp}
// CORRECT
enum class TokenType
{
    Access,
    Refresh
};

enum class ConnectionState
{
    Idle,
    Reading,
    Writing,
    Closing
};

TokenType eCurrentType = TokenType::Access;
ConnectionState m_eState = ConnectionState::Idle;

// WRONG - plain enum, uppercase values
enum TokenType { TOKEN_ACCESS, TOKEN_REFRESH };
{code}

---

## 3. Indentation and Formatting

### 3.1 Spaces Only

Do not use tabs anywhere in the codebase. Use spaces exclusively. Configure your editor to insert spaces when the Tab key is pressed.

### 3.2 Indent Width

All indentation levels are 4 spaces.

### 3.3 Brace Placement (Allman Style)

Opening curly braces are always placed on the line below the control statement, function signature, or class declaration. They are never placed on the same line. Closing braces align vertically with the opening brace.

{code:cpp}
void ProcessIncomingRequest(
    _in const Request& sRequest
)
{
    if (bIsValid)
    {
        ForwardToBackend(sRequest);
    }
    else
    {
        RejectRequest(sRequest);
    }
}
{code}

This applies to all constructs: if, else, for, while, do...while, switch, class, struct, namespace, try, catch, and lambda bodies.

### 3.4 Function Parameter Formatting

Every function parameter must be placed on its own separate line, indented 4 spaces from the function declaration. The opening parenthesis stays on the function name line, and the closing parenthesis aligns with the function declaration. This applies to both declarations and definitions.

{code:cpp}
// CORRECT - each parameter on its own line, indented 4 spaces
bool __stdcall ValidateJsonPayload(
    _in const std::string& szPayload,
    _in const Schema& sSchema
);

// Inside a class (parameters still 4 spaces from function line)
class Validator
{
    public:
        bool __thiscall ValidateRequest(
            _in const std::string& szVerb,
            _in const std::string& szPath,
            _out ErrorInfo& sError
        ) const;
};

// WRONG - parameters on the same line as function name
bool __stdcall ValidateJsonPayload(_in const std::string& szPayload, _in const Schema& sSchema);

// WRONG - parameters aligned to parenthesis
bool __stdcall ValidateJsonPayload(_in const std::string& szPayload,
                                   _in const Schema& sSchema);
{code}

### 3.5 Access Specifier Indentation

The public, protected, and private access specifiers inside a class or struct are indented by one level (4 spaces) from the class braces. Members and methods are then indented by a further level (8 spaces total from the class braces). This provides clear visual hierarchy: class, then access level, then members.

{code:cpp}
class RoutingTable
{
    public:
        void __thiscall RegisterEndpoint(
            _in const EndpointDefinition& sDefinition
        );

        bool __thiscall FindEndpoint(
            _in  const std::string& szPath,
            _out EndpointDefinition& sEndpoint
        ) const;

    protected:
        void __thiscall RebuildIndex();

    private:
        std::vector<EndpointDefinition>  m_stdsEndpoints;
        std::mutex                       m_stdMutexAccess;
        static const uint32_t            msc_un32MaxEndpoints = 4096;
};
{code}

---

## 4. Function Parameter Annotations

### 4.1 Direction Macros: _in, _out, _inout

Every function parameter must be prefixed with a direction annotation indicating how the parameter is used. These are preprocessor defines that resolve to nothing; they exist solely for readability and intent documentation.

{code:cpp}
#define _in
#define _out
#define _inout
{code}

| Annotation | Meaning |
|------------|---------|
| _in | Parameter is read by the function but not modified. Typically passed by const reference or by value. |
| _out | Parameter is written to by the function. The caller does not need to initialize it. Passed by reference or pointer. |
| _inout | Parameter is both read and modified by the function. Passed by reference or pointer. |

{code:cpp}
bool DecryptToken(
    _in  const std::string& szToken,
    _in  const AesKey& sKey,
    _out TokenPayload& sPayload
) const;

void UpdateConnectionStatistics(
    _in    uint32_t un32ConnectionId,
    _inout Statistics& sStatistics
);
{code}

---

## 5. Calling Convention Annotations

All function and method definitions include an explicit calling convention specifier. These are informational macros that resolve to nothing in this project. They document the intended calling semantics and make function signatures immediately recognizable.

{code:cpp}
#define __stdcall
#define __thiscall
#define __fastcall
{code}

Use __thiscall for class member functions and __stdcall for free functions and static functions.

{code:cpp}
// Free function
bool __stdcall ValidateJsonPayload(
    _in const std::string& szPayload,
    _in const Schema& sSchema
);

// Member function
bool __thiscall RoutingTable::FindEndpoint(
    _in  const std::string& szVerb,
    _in  const std::string& szPath,
    _out EndpointDefinition& sEndpoint
) const;
{code}

---

## 6. Control Flow Rules

### 6.1 Single Return Point

Functions must have exactly one return statement, and it must be the last statement in the function body. Use a result variable and conditional logic to converge on the single return point. Do not use early returns, guard clauses, or mid-function returns.

{code:cpp}
// CORRECT
bool __thiscall TokenEngine::ValidateAccessToken(
    _in const std::string& szToken
) const
{
    bool bResult = false;

    TokenPayload sPayload;
    if (DecryptToken(szToken, m_sKey, sPayload))
    {
        if ((sPayload.eTokenType == TokenType::Access) &&
            (sPayload.n64Expiry > GetCurrentTimestamp()))
        {
            bResult = true;
        }
    }

    return bResult;
}

// WRONG - multiple returns
bool ValidateAccessToken(
    const std::string& szToken
) const
{
    TokenPayload sPayload;
    if (!DecryptToken(szToken, m_sKey, sPayload))
    {
        return false;   // WRONG: early return
    }
    return (sPayload.eTokenType == TokenType::Access);
}
{code}

### 6.2 Mandatory Parentheses in Expressions

Always use explicit parentheses in conditional expressions, even when operator precedence is unambiguous. Developers must never need to mentally resolve precedence rules. Every binary comparison or logical operation within an if, while, or do...while condition must be parenthesized.

{code:cpp}
// CORRECT - every operation is explicitly parenthesized
if ((unAge >= 18) && (szCountry == "US"))
{
    GrantAccess();
}

while ((unRetryCount < c_unMaxRetries) && (!bIsConnected))
{
    AttemptConnection();
}

if (((unFlags & FlagMask::ReadOnly) != 0) ||
    ((unFlags & FlagMask::Locked) != 0))
{
    RejectModification();
}

// WRONG - missing parentheses around individual comparisons
if (unAge >= 18 && szCountry == "US")
{
    GrantAccess();
}
{code}

---

## 7. Const Correctness

Every class method that does not modify the state of the class must be declared const. This is not optional. If a method can be const, it must be const. The compiler will enforce this; reviewers will reject non-const methods that could be const.

{code:cpp}
class EndpointRegistry
{
    public:
        // These methods do not modify state - MUST be const
        bool __thiscall HasEndpoint(
            _in const std::string& szPath
        ) const;

        uint32_t __thiscall GetEndpointCount() const;

        // This method modifies state - not const
        void __thiscall RegisterEndpoint(
            _in const EndpointDefinition& sDefinition
        );
};
{code}

---

## 8. Header Guards

All header files must use #pragma once as the include guard. It must be the very first line of every header file, before any other code, comments, or includes.

{code:cpp}
// File: include/gateway/core/EventLoop.h
#pragma once

#include <cstdint>

class EventLoop
{
    // ...
};
{code}

{code:cpp}
// File: include/gateway/auth/TokenEngine.h
#pragma once

#include <string>

// ...
{code}

- Every .h file must have #pragma once. No exceptions.
- #pragma once must be the first line of the file (a file-level comment identifying the file is acceptable above it).
- Do not use #ifndef / #define / #endif guards. Use #pragma once exclusively.

---

## 9. Complete Example

The following example demonstrates all coding standards applied together:

{code:cpp}
#pragma once

#define _in
#define _out
#define _inout
#define __stdcall
#define __thiscall

#include <string>
#include <vector>
#include <cstdint>

enum class TokenType
{
    Access,
    Refresh
};

static const uint32_t sc_un32MaxTokenSize = 4096;
static const uint32_t sc_un32DefaultExpiry = 300;

class TokenEngine
{
    public:
        bool __thiscall GenerateTokenPair(
            _in  const std::string& szServerId,
            _in  const std::string& szUserId,
            _in  uint32_t un32AccessExpiry,
            _in  uint32_t un32RefreshExpiry,
            _out std::string& szAccessToken,
            _out std::string& szRefreshToken
        );

        bool __thiscall ValidateAccessToken(
            _in  const std::string& szToken,
            _in  const std::string& szExpectedServerId,
            _out std::string& szUserId
        ) const;

        bool __thiscall RefreshTokenPair(
            _in  const std::string& szRefreshToken,
            _out std::string& szNewAccessToken,
            _out std::string& szNewRefreshToken
        );

        uint32_t __thiscall GetActiveTokenCount() const;

    private:
        bool __thiscall EncryptPayload(
            _in  const TokenPayload& sPayload,
            _out std::string& szEncryptedToken
        ) const;

        bool __thiscall DecryptPayload(
            _in  const std::string& szEncryptedToken,
            _out TokenPayload& sPayload
        ) const;

        AesKey                      m_sEncryptionKey;
        std::string                 m_szDefaultServerId;
        std::vector<std::string>    m_stdszIssuedTokens;
        static const uint32_t       msc_un32MaxPayloadSize = 8192;
};

// ---- Implementation ----

bool __thiscall TokenEngine::ValidateAccessToken(
    _in  const std::string& szToken,
    _in  const std::string& szExpectedServerId,
    _out std::string& szUserId
) const
{
    bool bResult = false;

    if (szToken.size() <= sc_un32MaxTokenSize)
    {
        TokenPayload sPayload;

        if (DecryptPayload(szToken, sPayload))
        {
            int64_t n64CurrentTime = GetCurrentTimestamp();

            if ((sPayload.eTokenType == TokenType::Access) &&
                (sPayload.szServerId == szExpectedServerId) &&
                (sPayload.n64Expiry > n64CurrentTime))
            {
                szUserId = sPayload.szUserId;
                bResult = true;
            }
        }
    }

    return bResult;
}
{code}

---

## Quick Reference Checklist

- Hungarian notation on all variables: scope prefix (g_, s_, c_, m_, gc_, gs_, gsc_, sc_, ms_, mc_, msc_) + type prefix (un32, sz, b, e, std, etc.) + PascalCase name
- m_ prefix on all class/struct member variables; ms_ for static members, mc_ for const members, msc_ for static const members
- e prefix for all enum-typed variables; enum class only, no plain enums
- std prefix for all STL types (vectors, maps, sets, smart pointers, atomics, optionals, etc.); sz remains the shorthand for std::string
- p for pointers, pp for pointer-to-pointer, combined with the pointed-to type (pszBuffer, ppb8Data)
- Full descriptive words only: unIndex not i, szServerIdentifier not srvId
- Allman brace style: opening brace on its own line, always
- 4 spaces per indent level, no tabs anywhere
- Every function parameter on its own separate line, indented 4 spaces from the function declaration
- Access specifiers (public, private, protected) indented 4 spaces from class braces; members indented 8 spaces
- _in / _out / _inout on every function parameter
- __stdcall / __thiscall on every function definition
- Single return point: return only at the bottom of every function
- Explicit parentheses: parenthesize every comparison and logical operation
- const on every method that does not modify class state
- #pragma once on every .h file as the first line; no #ifndef/#define/#endif guards