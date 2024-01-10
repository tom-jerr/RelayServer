add_executable(BUFFERTEST BufferTest.cpp)
target_link_libraries(BUFFERTEST PRIVATE RelayLib)

add_executable(EPOLLTEST EpollTest.cpp)
target_link_libraries(EPOLLTEST PRIVATE RelayLib)