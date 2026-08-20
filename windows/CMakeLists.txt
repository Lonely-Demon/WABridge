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
    src/wabridge_display.cpp
    src/wabridge_coordinator.cpp
    src/wabridge_secure_coordinator.cpp
    src/wabridge_audio.cpp
    src/wabridge_input.cpp
    src/wabridge_identity.cpp
)
target_include_directories(wabridge_protocol PUBLIC include)
target_link_libraries(wabridge_protocol PUBLIC OpenSSL::SSL OpenSSL::Crypto)
if (WIN32)
    target_link_libraries(wabridge_protocol PUBLIC ws2_32 crypt32)
endif()

option(WABRIDGE_BUILD_QT_WINDOWS "Build the Windows Qt6 coordinator application" OFF)
if (WABRIDGE_BUILD_QT_WINDOWS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(wabridge_windows WIN32 src/wabridge_qt_main.cpp)
    target_link_libraries(wabridge_windows PRIVATE wabridge_protocol Qt6::Widgets)
    target_compile_options(wabridge_windows PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4;/WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Werror>
    )
endif()
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

add_executable(wabridge_display_test ../tests/test_cpp_display.cpp)
target_link_libraries(wabridge_display_test PRIVATE wabridge_protocol)

add_executable(wabridge_coordinator_test ../tests/test_cpp_coordinator.cpp)
target_link_libraries(wabridge_coordinator_test PRIVATE wabridge_protocol)

add_executable(wabridge_secure_coordinator_test ../tests/test_cpp_secure_coordinator.cpp)
target_link_libraries(wabridge_secure_coordinator_test PRIVATE wabridge_protocol)

add_executable(wabridge_audio_test ../tests/test_cpp_audio.cpp)
target_link_libraries(wabridge_audio_test PRIVATE wabridge_protocol)

add_executable(wabridge_input_test ../tests/test_cpp_input.cpp)
target_link_libraries(wabridge_input_test PRIVATE wabridge_protocol)

add_executable(wabridge_identity_test ../tests/test_cpp_identity.cpp)
target_link_libraries(wabridge_identity_test PRIVATE wabridge_protocol)

if (NOT MSVC)
    target_link_libraries(wabridge_tls_stream_test PRIVATE pthread)
endif()

enable_testing()
add_test(NAME wabridge_protocol_test COMMAND $<TARGET_FILE:wabridge_protocol_test>)
add_test(NAME wabridge_session_test COMMAND $<TARGET_FILE:wabridge_session_test>)
add_test(NAME wabridge_tls_test COMMAND $<TARGET_FILE:wabridge_tls_test>)
add_test(NAME wabridge_tls_handshake_test COMMAND $<TARGET_FILE:wabridge_tls_handshake_test>)
add_test(NAME wabridge_pairing_test COMMAND $<TARGET_FILE:wabridge_pairing_test>)
add_test(NAME wabridge_tls_stream_test COMMAND $<TARGET_FILE:wabridge_tls_stream_test>)
add_test(NAME wabridge_endpoint_test COMMAND $<TARGET_FILE:wabridge_endpoint_test>)
add_test(NAME wabridge_discovery_cache_test COMMAND $<TARGET_FILE:wabridge_discovery_cache_test>)
add_test(NAME wabridge_messages_test COMMAND $<TARGET_FILE:wabridge_messages_test>)
add_test(NAME wabridge_socket_test COMMAND $<TARGET_FILE:wabridge_socket_test>)
add_test(NAME wabridge_socket_tls_test COMMAND $<TARGET_FILE:wabridge_socket_tls_test>)
add_test(NAME wabridge_file_test COMMAND $<TARGET_FILE:wabridge_file_test>)
add_test(NAME wabridge_clipboard_test COMMAND $<TARGET_FILE:wabridge_clipboard_test>)
add_test(NAME wabridge_session_peer_test COMMAND $<TARGET_FILE:wabridge_session_peer_test>)
add_test(NAME wabridge_display_test COMMAND $<TARGET_FILE:wabridge_display_test>)
add_test(NAME wabridge_coordinator_test COMMAND $<TARGET_FILE:wabridge_coordinator_test>)
add_test(NAME wabridge_secure_coordinator_test COMMAND $<TARGET_FILE:wabridge_secure_coordinator_test>)
add_test(NAME wabridge_audio_test COMMAND $<TARGET_FILE:wabridge_audio_test>)
add_test(NAME wabridge_input_test COMMAND $<TARGET_FILE:wabridge_input_test>)
add_test(NAME wabridge_identity_test COMMAND $<TARGET_FILE:wabridge_identity_test>)
