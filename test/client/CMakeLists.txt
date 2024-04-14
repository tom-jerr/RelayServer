add_executable(SENDCLI send_cli.cpp)
target_link_libraries(SENDCLI PRIVATE RelayLib)

add_executable(RECVCLI recv_cli.cpp)
target_link_libraries(RECVCLI PRIVATE RelayLib)

# add_executable(TESTSERVER test_server.cpp)
# target_link_libraries(TESTSERVER PRIVATE RelayLib)

add_executable(SENDRECV send_recv_cli.cpp)
target_link_libraries(SENDRECV PRIVATE RelayLib)