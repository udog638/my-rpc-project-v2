cmake_minimum_required(VERSION 3.10)
project(myrpc VERSION 1.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(unofficial-zookeeper CONFIG REQUIRED)

set(RPC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/rpc_src)

add_executable(log_demo
    rpc_src/core/log_demo_main.cpp
)

add_executable(config_demo
    rpc_src/core/config_demo_main.cpp
    rpc_src/load_config/rpc_server_config.cpp
    rpc_src/load_config/rpc_client_config.cpp
    rpc_src/load_config/registry_config.cpp
    rpc_src/load_config/thread_pool_config.cpp
)

add_executable(serializer_demo
    rpc_src/core/serializer_demo_main.cpp
    protos/message.pb.cc
)

set(RPC_CORE_SOURCES
    rpc_src/load_config/rpc_server_config.cpp
    rpc_src/load_config/rpc_client_config.cpp
    rpc_src/load_config/registry_config.cpp
    rpc_src/load_config/thread_pool_config.cpp
    rpc_src/network/create_socket.cpp
    rpc_src/network/connection.cpp
    rpc_src/network/connection_manager.cpp
    rpc_src/network/message_cycle.cpp
    rpc_src/protocol/rpc_protocol.cpp
    rpc_src/compress_data/zstd_compress.cpp
    rpc_src/encrypt/aes_encrypt.cpp
    rpc_src/thread_pool/thread_pool.cpp
    rpc_src/thread_pool/thread_pool_singleton.cpp
    rpc_src/registry/service_registry.cpp
    rpc_src/conn_balancer/zk_conn_handler.cpp
    rpc_src/conn_balancer/load_balancer.cpp
)

add_executable(rpc_server
    rpc_src/core/servers_main.cpp
    ${RPC_CORE_SOURCES}
)

add_executable(rpc_client
    rpc_src/core/client_main.cpp
    rpc_src/core/rpc_client.cpp
    ${RPC_CORE_SOURCES}
)

add_executable(stress_test_client
    rpc_src/tools/stress_test_client.cpp
    rpc_src/core/rpc_client.cpp
    ${RPC_CORE_SOURCES}
)

add_executable(compress_encrypt_demo
    rpc_src/core/compress_encrypt_demo_main.cpp
    rpc_src/compress_data/zstd_compress.cpp
    rpc_src/encrypt/aes_encrypt.cpp
)

foreach(target log_demo config_demo)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/utils
        ${RPC_SOURCE_DIR}/load_config
    )

    target_link_libraries(${target} PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    )
endforeach()

target_include_directories(serializer_demo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/utils
    ${RPC_SOURCE_DIR}/serializer
    ${CMAKE_SOURCE_DIR}/protos
)

target_link_libraries(serializer_demo PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    protobuf::libprotobuf
)

foreach(target rpc_server rpc_client stress_test_client)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/utils
        ${RPC_SOURCE_DIR}/load_config
        ${RPC_SOURCE_DIR}/network
        ${RPC_SOURCE_DIR}/protocol
        ${RPC_SOURCE_DIR}/serializer
        ${RPC_SOURCE_DIR}/service
        ${RPC_SOURCE_DIR}/core
        ${RPC_SOURCE_DIR}/compress_data
        ${RPC_SOURCE_DIR}/encrypt
        ${RPC_SOURCE_DIR}/thread_pool
        ${RPC_SOURCE_DIR}/registry
        ${RPC_SOURCE_DIR}/conn_balancer
        ${CMAKE_SOURCE_DIR}/protos
    )

    target_link_libraries(${target} PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
        unofficial::zookeeper::zookeeper
    )

    target_compile_definitions(${target} PRIVATE
        THREADED
        ZK_DEPRECATED=1
    )
endforeach()

target_include_directories(compress_encrypt_demo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/utils
    ${RPC_SOURCE_DIR}/compress_data
    ${RPC_SOURCE_DIR}/encrypt
)

target_link_libraries(compress_encrypt_demo PRIVATE
    spdlog::spdlog
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
)
