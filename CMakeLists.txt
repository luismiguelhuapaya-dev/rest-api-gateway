cmake_minimum_required(VERSION 3.20)
project(RestApiGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

include_directories(${CMAKE_SOURCE_DIR}/include)

set(GATEWAY_SOURCES
    src/core/EventLoop.cpp
    src/core/Coroutine.cpp
    src/core/Configuration.cpp
    src/transport/TcpListener.cpp
    src/transport/HttpParser.cpp
    src/transport/HttpResponse.cpp
    src/transport/UnixSocketListener.cpp
    src/transport/FrameProtocol.cpp
    src/routing/RoutingTable.cpp
    src/routing/EndpointDefinition.cpp
    src/routing/ParameterSchema.cpp
    src/validation/ValidationEngine.cpp
    src/validation/JsonParser.cpp
    src/auth/TokenEngine.cpp
    src/auth/AesGcm.cpp
    src/forwarding/RequestFormatter.cpp
    src/forwarding/BackendForwarder.cpp
    src/logging/Logger.cpp
    src/main.cpp
)

add_executable(rest-api-gateway ${GATEWAY_SOURCES})

target_compile_features(rest-api-gateway PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rest-api-gateway PRIVATE -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(rest-api-gateway PRIVATE -fcoroutines-ts)
endif()
