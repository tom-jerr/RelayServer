add_executable(SENDCLI send_cli.cpp)
target_link_libraries(SENDCLI PRIVATE RelayLib)

add_executable(RECVCLI recv_cli.cpp)
target_link_libraries(RECVCLI PRIVATE RelayLib)