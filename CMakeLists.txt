cmake_minimum_required(VERSION 3.10)
project(p2p_sync)
set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${nlohmann_json_INCLUDE_DIRS}
)

add_executable(p2p_sync
    src/main.cpp
    src/network.cpp
    src/crypto.cpp
    src/versioning.cpp
)

target_link_libraries(p2p_sync
    ${Boost_SYSTEM_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

