#include "../include/Server.h"

int main() {
  EpollEvent epoll;
  epoll.StartEpoll("127.0.0.1", 1234);
  return 0;
}