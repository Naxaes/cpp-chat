cmake_minimum_required(VERSION 3.14)
project(Chat)

set(CMAKE_CXX_STANDARD 14)

add_executable(SimplexServer server_example.cpp)
add_executable(SimplexClient client_example.cpp)


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(DuplexClient duplex_client_example.cpp)
add_executable(DuplexServer duplex_server_example.cpp)

target_link_libraries(DuplexClient Threads::Threads)
target_link_libraries(DuplexServer Threads::Threads)


add_executable(Client client.cpp)
add_executable(Server server.cpp)

target_link_libraries(Client Threads::Threads)
target_link_libraries(Server Threads::Threads)
