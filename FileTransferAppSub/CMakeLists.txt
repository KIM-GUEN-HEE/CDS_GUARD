cmake_minimum_required(VERSION 3.16)

project(FILE_TRANSFER_APP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Network)

include(FetchContent)
FetchContent_Declare(
    asio 
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG master
)
FetchContent_MakeAvailable(asio)

find_package(OpenSSL QUIET)

include_directories(include ${asio_SOURCE_DIR}/asio/include)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_resources(APP_RESOURCES resources.qrc)

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

qt_add_qml_module(FILE_TRANSFER_UI_QML_MODULE
    URI CDSFileTransferAppUI
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CDSFileTransferAppUI"
    QML_FILES
        qml/Main.qml
        qml/App.qml
        qml/Screen01.qml
)

qt_add_executable(FILE_TRANSFER_APP
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
    ${APP_RESOURCES}
)

target_include_directories(FILE_TRANSFER_APP
        PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (OpenSSL_FOUND)
    target_compile_definitions(FILE_TRANSFER_APP PRIVATE USE_OPENSSL_HMAC)
    target_link_libraries(FILE_TRANSFER_APP PRIVATE OpenSSL::Crypto)
else()
    message(STATUS "OpenSSL not found. Compiling with Qt-based HMAC.")
endif()

target_link_libraries(FILE_TRANSFER_APP
    PRIVATE Qt6::Quick FILE_TRANSFER_UI_QML_MODULE
)
