cmake_minimum_required(VERSION 3.16)
project(CDS_GUARD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ───────────────── file glob ─────────────────
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE PACKET_PROCESSOR_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/include/*.h"
)

file(GLOB_RECURSE PACKET_PROCESSOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/src/*.cpp"
)

list(REMOVE_ITEM PACKET_PROCESSOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/src/main.cpp")

# ───────────────── Fetch ─────────────────
include(FetchContent)
# ───────────────── asio ──────────────────

FetchContent_Declare(
    asio 
    GIT_REPOSITORY 
    https://github.com/chriskohlhoff/asio.git
)
FetchContent_MakeAvailable(asio)

include_directories(include ${asio_SOURCE_DIR}/asio/include)

# ──────────────── Open SSL ───────────────

find_package(OpenSSL QUIET)

if (NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found – fetching opentls project (static, minimal)…")
    FetchContent_Declare(
        openssl-cmake
        GIT_REPOSITORY https://github.com/viaduck/openssl-cmake.git
    )
    FetchContent_MakeAvailable(openssl-cmake)   # targets: OpenSSL::SSL OpenSSL::Crypto
    set(OpenSSL_FOUND TRUE)
endif()

if (NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL could not be found or built")
endif()
    
# ──────────────── 앱 타깃 ────────────────
add_executable(CDS_GUARD
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
    ${PACKET_PROCESSOR_HEADERS}
    ${PACKET_PROCESSOR_SOURCES}
)

target_include_directories(
    CDS_GUARD
        PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../PacketTransport/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/include
)

target_link_libraries(CDS_GUARD
    PRIVATE
        pthread
        OpenSSL::Crypto
)
