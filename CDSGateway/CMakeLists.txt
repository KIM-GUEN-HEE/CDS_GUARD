cmake_minimum_required(VERSION 3.16)
project(CDS_GATEWAY LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

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

#____________set qt path______________
set(CMAKE_PREFIX_PATH "/opt/Qt/6.10.1/gcc_64")

# ───────────────── find_package ─────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs img_hash)
# find_package(OpenCV ...) 아래에 추가
message(STATUS "-----------------------------------------")
message(STATUS "OpenCV_FOUND: ${OpenCV_FOUND}")
message(STATUS "OpenCV_INCLUDE_DIRS: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV_LIBS: ${OpenCV_LIBS}")
message(STATUS "-----------------------------------------")
qt_standard_project_setup()

# ───────────────── Fetch ─────────────────
include(FetchContent)

include_directories(${OpenCV_INCLUDE_DIRS})

# ───────────────── asio ──────────────────
include(FetchContent)
FetchContent_Declare(
    asio 
    GIT_REPOSITORY 
    https://github.com/chriskohlhoff/asio.git
)
FetchContent_MakeAvailable(asio)

include_directories(include ${asio_SOURCE_DIR}/asio/include)

# ──────────────── nlohmann/json ───────────────
FetchContent_Declare(
    nlohmann_json       # FetchContent 내에서 사용할 이름
    GIT_REPOSITORY      https://github.com/nlohmann/json.git
    GIT_TAG             master  # 특정 릴리즈 태그를 사용하는 것이 안정적입니다
)

FetchContent_MakeAvailable(nlohmann_json)

# ──────────────── Open SSL ───────────────
find_package(OpenSSL QUIET)

if (NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found - fetching opentls project (static, minimal)…")
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

# ──────────────── 1. zlib-ng ────────────────
FetchContent_Declare(
    zlib-ng
    GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng.git
    GIT_TAG stable
)
set(ZLIB_ENABLE_TESTS   OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIB_COMPAT         ON  CACHE BOOL "" FORCE) 
FetchContent_MakeAvailable(zlib-ng)

# 에러 해결 핵심: 
# zlib-ng 프로젝트의 실제 타겟은 'zlibng'입니다.
# 'zlib'는 이미 zlibng를 가리키는 ALIAS일 확률이 높습니다.
if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlibng)
        add_library(ZLIB::ZLIB ALIAS zlibng)
    elseif(TARGET zlib)
        # 만약 zlib가 ALIAS가 아니라 실제 타겟인 경우에만 성공함
        # 이미 위에서 에러가 났다면 이 블록은 건너뛰는 것이 안전합니다.
    endif()
endif()

# ALIAS 타겟에는 설정을 추가할 수 없으므로, 
# 꼭 필요하다면 실제 타겟인 zlibng에 추가해야 합니다.
if(TARGET zlibng)
    target_include_directories(zlibng
        INTERFACE
            $<BUILD_INTERFACE:${zlib_BINARY_DIR}>
    )
endif()

list(PREPEND CMAKE_INCLUDE_PATH "${zlib_BINARY_DIR}")
list(PREPEND CMAKE_LIBRARY_PATH "${zlib_BINARY_DIR}")

# ── (3) 변수도 덮어써 두면 안전 (MODULE 모드가 그대로 씀) ───
set(ZLIB_FOUND        TRUE                               CACHE BOOL      "" FORCE)
set(ZLIB_INCLUDE_DIR  "${zlib_BINARY_DIR}"               CACHE PATH      "" FORCE)
set(ZLIB_INCLUDE_DIRS "${zlib_BINARY_DIR}"               CACHE PATH      "" FORCE)
set(ZLIB_LIBRARY      "$<TARGET_FILE:zlib>"              CACHE FILEPATH  "" FORCE)

# ──────────────── 2. minizip ────────────────
set(MZ_ZLIB             ON  CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS       OFF CACHE BOOL "" FORCE)
set(MZ_FORCE_FETCH_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    minizip
    GIT_REPOSITORY https://github.com/nmoinvaz/minizip.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minizip)
    
# ──────────────── 앱 타깃 ────────────────
qt_add_resources(APP_RESOURCES resources.qrc)

qt_add_executable(CDS_GATEWAY
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
    ${APP_RESOURCES}
    ${PACKET_PROCESSOR_HEADERS}
    ${PACKET_PROCESSOR_SOURCES}
)

target_include_directories(
    CDS_GATEWAY
        PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../PacketTransport/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../PacketProcessor/include
            ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(CDS_GATEWAY
    PRIVATE
        Qt6::Widgets
        zlib            # 실제 라이브러리
        minizip
        OpenSSL::Crypto
        Qt6::Network
        nlohmann_json
        ${OpenCV_LIBS}
)
