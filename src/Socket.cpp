#include "Socket.h"

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

Socket::Socket(int fd, Logger *logger) : fd_(fd), logger_(logger) {}

Socket::~Socket() {
  if (fd_ != -1) {
    close(fd_);
  }
}

int Socket::GetFd() const { return fd_; }

// Connect to server
void Socket::Connect(NetAddress &server_addr) {
  if (fd_ == -1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
  }
  if (connect(fd_, server_addr.GetAddr(), sizeof(struct sockaddr_in)) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket Connect() Error\n");
    throw std::logic_error("Socket: Connect() Error");
  }
}

// Bind to address
void Socket::Bind(NetAddress &server_addr, bool reusable) {
  if (fd_ == -1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
  }
  if (reusable) {
    SetReusable();
  }
  if (bind(fd_, server_addr.GetAddr(), sizeof(struct sockaddr_in)) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket Bind() Error\n");
    throw std::logic_error("Socket: Bind() Error");
  }
}

// Listen need to connect sockets
void Socket::Listen() const {
  assert(fd_ != -1 && "cannot Listen() with an invalid fd");
  if (listen(fd_, BACKLOG) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket Listen() Error\n");
    throw std::logic_error("Socket: Listen() Error");
  }
}

// accept conneted socket(nonblocking)
int Socket::Accept(NetAddress &client_addr) {
  assert(fd_ != -1 && "cannot Accept() with an invalid fd");
  int client_fd = accept(fd_, client_addr.GetAddr(), client_addr.GetAddrLen());
  if (client_fd == -1) {
    // under high pressure, accept might fail.
    // but server should not fail at this time
    logger_->Log(Logger::WARNING, "RelayServer: Socket Accept() failed\n");
  }
  return client_fd;
}

// set REUSEPORT and REUSEADDR
void Socket::SetReusable() {
  int opt = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket SetReusable() Error\n");
    throw std::logic_error("Socket: SetReusable() Error");
  }
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket SetReusable() Error\n");
    throw std::logic_error("Socket: SetReusable() Error");
  }
}

// set socket nonblocking
void Socket::SetNonBlocking() {
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket SetNonBlocking() Error\n");
    throw std::logic_error("Socket: SetNonBlocking() Error");
  }
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
    logger_->Log(Logger::ERROR, "RelayServer: Socket SetNonBlocking() Error\n");
    throw std::logic_error("Socket: SetNonBlocking() Error");
  }
}