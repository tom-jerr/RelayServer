#include "../include/Utils.h"

#include <bits/types/struct_timespec.h>
#include <byteswap.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

uint64_t ntoh64(uint64_t net64) {
  if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return bswap_64(net64);
  else
    return net64;
}

uint64_t hton64(uint64_t host64) {
  if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return bswap_64(host64);
  else
    return host64;
}
std::pair<uint16_t, uint32_t> ParseHeader(HeaderInfo* header,
                                          const size_t& id) {
  uint16_t msglen = ntohs(header->len);
  uint32_t header_cliID = ntohl(header->cliId);
  return {msglen, header_cliID};
}

struct timespec GetHeader(uint16_t length, uint32_t id, HeaderInfo* header) {
  header->len = htons(length);
  header->cliId = htonl(id);
  struct timespec timestamp;
  clock_gettime(CLOCK_REALTIME, &timestamp);
  header->sec = hton64(timestamp.tv_sec);
  header->nsec = hton64(timestamp.tv_nsec);
  return timestamp;
}

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

int Write(int fd, const void* buffer, size_t len) {
  int ret = write(fd, buffer, len);
  ASSERT(ret != -1);
  return ret;
}

int Read(int fd, void* buffer, size_t len) {
  int ret = read(fd, buffer, len);
  ASSERT(ret != -1);
  return ret;
}

int Close(int fd) {
  int ret = close(fd);
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

void AddOutFd(int epoll_fd, int fd) {
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLOUT;
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