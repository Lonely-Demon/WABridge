cmake_minimum_required(VERSION 3.20)
project(WABridgeWindowsCore LANGUAGES CXX)

find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wabridge_protocol STATIC
    src/wabridge_envelope.cpp
    src/wabridge_session.cpp
    src/wabridge_tls.cpp
    src/wabridge_tls_stream.cpp
    src/wabridge_pairing.cpp
    src/wabridge_endpoint.cpp
    src/wabridge_discovery_cache.cpp
    src/wabridge_messages.cpp
    src/wabridge_socket.cpp
    src/wabridge_file.cpp
    src/wabridge_clipboard.cpp
    src/wabridge_session_peer.cpp
)
target_include_directories(wabridge_protocol PUBLIC include)
target_link_libraries(wabridge_protocol PUBLIC OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(wabridge_protocol PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(wabridge_protocol_test ../tests/test_cpp_envelope.cpp)
target_link_libraries(wabridge_protocol_test PRIVATE wabridge_protocol)

add_executable(wabridge_session_test ../tests/test_cpp_session.cpp)
target_link_libraries(wabridge_session_test PRIVATE wabridge_protocol)

add_executable(wabridge_tls_test ../tests/test_cpp_tls.cpp)
target_link_libraries(wabridge_tls_test PRIVATE wabridge_protocol)

add_executable(wabridge_tls_handshake_test ../tests/test_cpp_tls_handshake.cpp)
target_link_libraries(wabridge_tls_handshake_test PRIVATE wabridge_protocol)

add_executable(wabridge_pairing_test ../tests/test_cpp_pairing.cpp)
target_link_libraries(wabridge_pairing_test PRIVATE wabridge_protocol)

add_executable(wabridge_tls_stream_test ../tests/test_cpp_tls_stream.cpp)
target_link_libraries(wabridge_tls_stream_test PRIVATE wabridge_protocol)

add_executable(wabridge_endpoint_test ../tests/test_cpp_endpoint.cpp)
target_link_libraries(wabridge_endpoint_test PRIVATE wabridge_protocol)

add_executable(wabridge_discovery_cache_test ../tests/test_cpp_discovery_cache.cpp)
target_link_libraries(wabridge_discovery_cache_test PRIVATE wabridge_protocol)

add_executable(wabridge_messages_test ../tests/test_cpp_messages.cpp)
target_link_libraries(wabridge_messages_test PRIVATE wabridge_protocol)

add_executable(wabridge_socket_test ../tests/test_cpp_socket.cpp)
target_link_libraries(wabridge_socket_test PRIVATE wabridge_protocol)

add_executable(wabridge_socket_tls_test ../tests/test_cpp_socket_tls.cpp)
target_link_libraries(wabridge_socket_tls_test PRIVATE wabridge_protocol)

add_executable(wabridge_file_test ../tests/test_cpp_file.cpp)
target_link_libraries(wabridge_file_test PRIVATE wabridge_protocol)

add_executable(wabridge_clipboard_test ../tests/test_cpp_clipboard.cpp)
target_link_libraries(wabridge_clipboard_test PRIVATE wabridge_protocol)

add_executable(wabridge_session_peer_test ../tests/test_cpp_session_peer.cpp)
target_link_libraries(wabridge_session_peer_test PRIVATE wabridge_protocol)

if (NOT MSVC)
    target_link_libraries(wabridge_tls_stream_test PRIVATE pthread)
endif()

enable_testing()
add_test(NAME wabridge_protocol_test COMMAND wabridge_protocol_test)
add_test(NAME wabridge_session_test COMMAND wabridge_session_test)
add_test(NAME wabridge_tls_test COMMAND wabridge_tls_test)
add_test(NAME wabridge_tls_handshake_test COMMAND wabridge_tls_handshake_test)
add_test(NAME wabridge_pairing_test COMMAND wabridge_pairing_test)
add_test(NAME wabridge_tls_stream_test COMMAND wabridge_tls_stream_test)
add_test(NAME wabridge_endpoint_test COMMAND wabridge_endpoint_test)
add_test(NAME wabridge_discovery_cache_test COMMAND wabridge_discovery_cache_test)
add_test(NAME wabridge_messages_test COMMAND wabridge_messages_test)
add_test(NAME wabridge_socket_test COMMAND wabridge_socket_test)
add_test(NAME wabridge_socket_tls_test COMMAND wabridge_socket_tls_test)
add_test(NAME wabridge_file_test COMMAND wabridge_file_test)
add_test(NAME wabridge_clipboard_test COMMAND wabridge_clipboard_test)
add_test(NAME wabridge_session_peer_test COMMAND wabridge_session_peer_test)
