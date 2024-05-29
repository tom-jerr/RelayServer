#include "Socket.h"

#include <asm-generic/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <stdexcept>

#include "Logger.h"

// 监听队列长度
constexpr int BACKLOG = 2000;

NSocket::NSocket(int fd) : fd_(fd) {}

NSocket::~NSocket() {
  if (fd_ != -1) {
    close(fd_);
  }
}

int NSocket::GetFd() const { return fd_; }

// Connect to server
void NSocket::Connect(NetAddress &server_addr) {
  if (fd_ == -1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
  }
  if (connect(fd_, server_addr.GetAddr(), sizeof(struct sockaddr_in)) == -1) {
    printf("RelayServer: NSocket Connect() Error\n");
    throw std::logic_error("NSocket: Connect() Error");
  }
}

// Bind to address
void NSocket::Bind(NetAddress &server_addr, bool reusable) {
  if (fd_ == -1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
  }
  // int value = 1;
  // setsockopt(fd_, SOL_SOCKET, MSG_NOSIGNAL, &value, sizeof(value));
  if (reusable) {
    SetReusable();
  }
  if (bind(fd_, server_addr.GetAddr(), sizeof(struct sockaddr_in)) == -1) {
    printf("RelayServer: NSocket Bind() Error\n");
    throw std::logic_error("NSocket: Bind() Error");
  }
}

// Listen need to connect NSockets
void NSocket::Listen() const {
  assert(fd_ != -1 && "cannot Listen() with an invalid fd");
  if (listen(fd_, BACKLOG) == -1) {
    printf("RelayServer: NSocket Listen() Error\n");
    throw std::logic_error("NSocket: Listen() Error");
  }
}

// accept conneted NSocket(nonblocking)
int NSocket::Accept(NetAddress &client_addr) {
  assert(fd_ != -1 && "cannot Accept() with an invalid fd");
  int client_fd;
  do {
    client_fd = accept4(fd_, client_addr.GetAddr(), client_addr.GetAddrLen(),
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd == -1) {
      if ((errno == ECONNABORTED) || (errno == EWOULDBLOCK) ||
          (errno == EINTR) || (errno == EPROTO)) {
        // 已经处理完全部连接
        break;
      } else {
        // 其他错误
        printf("RelayServer:\tACCEPT ERROR: exit with error\n");
        return -1;
      }
    }
    // 说明已经拿到clientfd
    return client_fd;
  } while (1);
  // 永远不应该走到这
  return -1;
}

// set REUSEPORT and REUSEADDR
void NSocket::SetReusable() {
  int opt = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    printf("RelayServer: NSocket SetReusable() Error\n");
    throw std::logic_error("NSocket: SetReusable() Error");
  }
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
    printf("RelayServer: NSocket SetReusable() Error\n");
    throw std::logic_error("NSocket: SetReusable() Error");
  }
}

// set NSocket nonblocking
void NSocket::SetNonBlocking() {
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1) {
    printf("RelayServer: NSocket SetNonBlocking() Error\n");
    throw std::logic_error("NSocket: SetNonBlocking() Error");
  }
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
    printf("RelayServer: NSocket SetNonBlocking() Error\n");
    throw std::logic_error("NSocket: SetNonBlocking() Error");
  }
}