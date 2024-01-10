#include "../include/Utils.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int InetPton(int af, const char* src, void* dst) {
  int ret = inet_pton(af, src, dst);
  ASSERT(ret > 0);
  return ret;
}

int SetNonBlocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  int ret = fcntl(fd, F_SETFL, new_option);
  if (ret == -1) {
    close(fd);
  }
  ASSERT(ret != -1);
  return old_option;
}

int Socket(int domain, int type, int protocol) {
  int ret = socket(domain, type, protocol);
  ASSERT(ret != -1);
  return ret;
}

int Bind(int fd, const struct sockaddr* servaddr, socklen_t len) {
  int ret = bind(fd, servaddr, len);
  if (ret == -1) {
    close(fd);
  }
  ASSERT(ret != -1);
  return ret;
}

int Listen(int fd, int n) {
  int ret = listen(fd, BACKLOG);
  if (ret == -1) {
    close(fd);
  }
  ASSERT(ret != -1);
  return ret;
}

int Connect(int fd, const struct sockaddr* servaddr, socklen_t len) {
  int ret = connect(fd, servaddr, len);
  if (ret == -1) {
    close(fd);
  }
  ASSERT(ret != -1);
  return ret;
}

void AddFd(int epoll_fd, int fd, bool enable_out, bool enable_et) {
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN;
  if (enable_out) {
    event.events |= EPOLLOUT;
  }
  if (enable_et) {
    event.events |= EPOLLET;
  }
  int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
  ASSERT(ret != -1);
}

void ModFd(int epoll_fd, int fd, int ev) {
  struct epoll_event event;
  if (ev & EPOLLIN) {
    event.events = EPOLLIN;
  }
  if (ev & EPOLLOUT) {
    event.events |= EPOLLOUT;
  }
  event.data.fd = fd;
  int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
  ASSERT(ret != -1);
}

void DelFd(int epoll_fd, int fd) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
  // ASSERT(ret != -1);
}