cmake_minimum_required(VERSION 3.20)
project(WABridgeWindowsCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wabridge_protocol STATIC
    src/wabridge_envelope.cpp
    src/wabridge_session.cpp
)
target_include_directories(wabridge_protocol PUBLIC include)
target_compile_options(wabridge_protocol PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(wabridge_protocol_test ../tests/test_cpp_envelope.cpp)
target_link_libraries(wabridge_protocol_test PRIVATE wabridge_protocol)

add_executable(wabridge_session_test ../tests/test_cpp_session.cpp)
target_link_libraries(wabridge_session_test PRIVATE wabridge_protocol)

enable_testing()
add_test(NAME wabridge_protocol_test COMMAND wabridge_protocol_test)
add_test(NAME wabridge_session_test COMMAND wabridge_session_test)
