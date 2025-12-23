cmake_minimum_required(VERSION 3.10)

project(ARIAAlgorithmTestProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(ARIA_SRC_DIR "${ROOT_DIR}/src/encryption")
set(ARIA_HEADER_DIR "${ROOT_DIR}/include")

set(TEST_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/ARIAAlgorithmTest.cpp"
)

set(ARIA_SOURCES
    "${ARIA_SRC_DIR}/ARIAAlgorithm.cpp"
    "${ARIA_SRC_DIR}/ARIAReference.cpp"
)

add_executable(ARIAAlgorithmTest
    ${ARIA_SOURCES}
    ${TEST_SRC}
)

target_include_directories(ARIAAlgorithmTest PRIVATE
    "${ARIA_HEADER_DIR}"
)

target_compile_definitions(ARIAAlgorithmTest PRIVATE DEBUG)

enable_testing()
add_test(NAME ARIAAlgorithm_CBC_Test COMMAND ARIAAlgorithmTest)
