cmake_minimum_required(VERSION 3.10)
project(PacketProcessor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# debug option
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()

# include
include_directories(${PROJECT_SOURCE_DIR}/include)

# reference, ARIA
add_library(aria_ref STATIC
    src/encryption/ARIAReference.cpp
)

# algorithm, ARIA
add_library(algorithm STATIC
    src/encryption/ARIAAlgorithm.cpp
)
target_link_libraries(algorithm PUBLIC aria_ref)

# protocol library
add_library(protocol STATIC
    src/protocol/ProtocolEngine.cpp
    src/protocol/ShiftModule.cpp
    src/protocol/PaddingModule.cpp
    src/protocol/EncryptionModule.cpp
    src/PacketBuffer.cpp
)
target_link_libraries(protocol PUBLIC algorithm)

# transport library
add_library(transport STATIC
    src/transport/L3SocketTransport.cpp
    src/transport/L2SocketTransport.cpp
)

# executable
add_executable(${PROJECT_NAME} src/main.cpp)

# link library
target_link_libraries(${PROJECT_NAME} protocol transport)
