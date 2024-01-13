
#include <arpa/inet.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <csignal>
#include <iostream>

#include "../include/Logger.h"
#include "../include/Server.h"
#include "../include/Utils.h"

int EpollEvent::exit_flag_ = 0;  // 需要在外部进行声明-zx
static size_t start_idx = 0;     // 读取报文的起始位置

EpollEvent::EpollEvent() : epoll_sockfd_(-1), epoll_fd_(-1) {
  Signal(SIGINT, SigIntHandler);
  logger_ = new Logger("log.txt");
}

EpollEvent::~EpollEvent() {
  logger_->Log(Logger::INFO, "EpollEvent is destructed\n");
  int ret = 1;
  if (epoll_sockfd_ != -1) {
    close(epoll_sockfd_);
    ASSERT(ret == 1);
  }
  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

void EpollEvent::UpdateNextClientId() {
  for (size_t i = next_client_id_;; ++i) {
    if (client_map_.find(i) == client_map_.end()) {
      next_client_id_ = i;
      return;
    }
  }
}

int EpollEvent::AddNewClient(MessageInfo *client) {
  client->header.cliId = next_client_id_;
  UpdateNextClientId();
  client_map_[client->header.connfd] = client;
  AddFd(epoll_fd_, client->header.connfd, 1, 0);
  logger_->Log(Logger::INFO, "EpollEvent:\tClient %d is added\t(%d in total)\n",
               next_client_id_, client_map_.size());
  return 0;
}

int EpollEvent::RemoveClient(const int &connfd) {
  ASSERT(client_map_.find(connfd) != client_map_.end());
  size_t cliId = client_map_[connfd]->header.cliId;
  client_map_.erase(connfd);
  if (cliId < next_client_id_) {
    next_client_id_ = cliId;
  }
  int ret = close(connfd);
  ASSERT(ret >= 0);
  DelFd(epoll_fd_, connfd);
  logger_->Log(Logger::INFO,
               "EpollEvent:\tClient %d is removed\t(%d in total)\n", cliId,
               client_map_.size());
  return 0;
}

int EpollEvent::RecvData(MessageInfo *msg, char *buffer, const size_t &len,
                         const size_t &id) {
  // 获取头部和连接描述符
  HeaderInfo *header = &msg->header;
  int connfd = header->connfd;
  /*
    非阻塞接收数据
  */
  int ret = recv(connfd, buffer, len, 0);
  if (ret < 0) {
    if (errno != EWOULDBLOCK) {
      std::cout << "Read error" << std::endl;
      logger_->Log(Logger::ERROR,
                   "RECV ERROR: EpollEvent:\tclient %d recv error\n", id);
      return -1;
    }
    /*
      记录EWOULDBLOCK事件
    */
    logger_->Log(Logger::ERROR,
                 "EpollEvent:\tEWOULDBLOCK ERROR: client %d recv EWOULDBLOCK\n",
                 id);
  } else if (ret == 0) {
    /*
      客户端关闭连接
    */
    shutdown(connfd, SHUT_WR);  // 不能向套接字写数据
    RemoveClient(connfd);
    return 0;
  } else {
    /*
      接收报文
    */
    if (ret >= len) {
      /*
        可以接收全部报文，如果执行只会执行一次
      */
      header->head_recv = true;  // 标记已经接收到报文头，下次接收报文体
      memcpy((char *)header, buffer, HEADERSZ);
      header->recv_idx += HEADERSZ;

      header->body_recv = true;  // 标记已经接收到报文体，直接结束过程
      memcpy(buffer, buffer + HEADERSZ, len - HEADERSZ);
      header->recv_idx += len - HEADERSZ;
      ASSERT(header->recv_idx == header->len - 1);
    } else if (ret >= HEADERSZ) {
      /*
        可以接收完整报文头和部分报文体
      */
      if (!header->head_recv) {
        header->head_recv = true;  // 标记已经接收到报文头，下次接收报文体
        memcpy((char *)header, buffer, HEADERSZ);
        header->recv_idx += HEADERSZ;
      }
      if (ret > HEADERSZ) {
        /*
          接收到部分报文体
        */
        memcpy(buffer, buffer + HEADERSZ, ret - HEADERSZ);
        header->recv_idx += ret - HEADERSZ;
      } else {
        /*
          没有空间接收报文头，记录No space error
        */
        logger_->Log(Logger::ERROR,
                     "EpollEvent:\tNO SPACE ERROR: client %d has no space to "
                     "recv error\n",
                     id);
      }
    }
  }
  return ret;
}

int EpollEvent::SendData(MessageInfo *msg, const char *buffer,
                         const size_t &len, const size_t &id) {
  int connfd = msg->header.connfd;
  int ret = write(connfd, buffer, len);
  if (ret < 0) {
    if (errno != EWOULDBLOCK) {
      logger_->Log(Logger::ERROR,
                   "EpollEvent:\tSEND ERROR: client %d send error\n", id);
      return -1;
    }
  }
  return ret;
}

void EpollEvent::StartEpoll(const std::string &ipaddr, const int port) {
  /*
    epoll 处理事件集
  */
  struct epoll_event events[MAXEVENTS];

  /*
    设置端口和地址
  */
  epoll_port_ = port;
  bzero(&servaddr_, sizeof(servaddr_));
  /*
    创建epoll_sockfd_并绑定、监听
  */
  epoll_sockfd_ = Socket(AF_INET, SOCK_STREAM, 0);

  servaddr_.sin_family = AF_INET;
  InetPton(AF_INET, ipaddr.c_str(), (void *)&servaddr_.sin_addr);
  // std::cout << "inet_pton:\t" << ret << std::endl;
  servaddr_.sin_port = htons(epoll_port_);
  Bind(epoll_sockfd_, (struct sockaddr *)&servaddr_, sizeof(servaddr_));
  logger_->Log(Logger::INFO, "EpollEvent:\tBind to %s:%d\n", ipaddr.c_str(),
               epoll_port_);
  Listen(epoll_sockfd_, BACKLOG);
  logger_->Log(Logger::INFO, "EpollEvent:\tListen on port %s:%d\n",
               ipaddr.c_str(), epoll_port_);
  /*
    创建epoll_fd
  */
  epoll_fd_ = epoll_create(MAXEVENTS);
  ASSERT(epoll_fd_ != -1);

  /*
    将epoll_sockfd_添加到epoll_fd_中
  */
  AddFd(epoll_fd_, epoll_sockfd_, 1, 0);
  /*
    设置epoll_sockfd_为非阻塞
  */
  SetNonBlocking(epoll_sockfd_);
  /*
    epoll event loop
  */
  for (;;) {
    // 不超时等待事件发生
    int nfds = epoll_wait(epoll_fd_, events, MAXEVENTS, -1);
    if (nfds < 0) {
      if (exit_flag_) {
        ExitEpoll();
        break;
      }
      logger_->Log(Logger::ERROR,
                   "EpollEvent:\tepoll_wait error, exit with error\n");
      CloseAllFd();
    }
    if (EventsHandler(events, nfds) < 0) {
      logger_->Log(Logger::ERROR,
                   "EpollEvent:\tepoll handler error, exit with error\n");
      CloseAllFd();
    }
    if (exit_flag_) {
      ExitEpoll();
      break;
    }
  }
}

int EpollEvent::EventsHandler(struct epoll_event *events, const int &nready) {
  for (int i = 0; i < nready; ++i) {
    int sockfd = events[i].data.fd;
    if (sockfd == epoll_sockfd_) {
      /*
        有新的连接
      */
      struct sockaddr_in clientaddr;
      socklen_t len = sizeof(clientaddr);

      // 非阻塞accept
      for (;;) {
        int connfd =
            accept(epoll_sockfd_, (struct sockaddr *)&clientaddr, &len);
        if (connfd == -1) {
          if ((errno == ECONNABORTED) || (errno == EWOULDBLOCK) ||
              (errno == EINTR) || (errno == EPROTO)) {
            // 已经处理完全部连接
            break;
          } else {
            // 其他错误
            logger_->Log(Logger::ERROR,
                         "EpollEvent:\taccept error, exit with error\n");
            return -1;
          }
        }
        /*
          声明一个新的客户端，连接描述符为connfd
        */
        MessageInfo *client = new MessageInfo;
        client->header.connfd = connfd;
        /*
          设置connfd为非阻塞
        */
        SetNonBlocking(connfd);
        /*
          向服务器添加新的客户端
        */
        AddNewClient(client);
      }
    } else {
      /*
        已连接套接字
      */
      ASSERT(client_map_.find(sockfd) != client_map_.end());
      int cur_client = client_map_[sockfd]->header.cliId;
      MessageInfo *cur_client_msg = client_map_[sockfd];
      int dst_client = GetDstId(cur_client);

      MessageInfo *dst_client_msg = nullptr;
      if (client_map_.find(dst_client) != client_map_.end()) {
        dst_client_msg = client_map_[dst_client];
      }
      if (dst_client_msg == nullptr) {
        /*
          目的客户端不存在
        */
        std::cout << "Dst client not exist" << std::endl;
      }
      /*
        Epoll有可读事件
      */
      if (events[i].events & EPOLLIN) {
        /*
          读取报头
        */
        ssize_t n = RecvData(cur_client_msg, (char *)&cur_client_msg->header,
                             sizeof(HeaderInfo), cur_client);
        if (n <= 0) {
          continue;
        }
        /*
          读取报文
        */
        n = RecvData(cur_client_msg, cur_client_msg->buffer,
                     cur_client_msg->header.len, cur_client);
        if (n <= 0) {
          continue;
        }
      }
      /*
        Epoll有可写事件
      */
      if (events[i].events & EPOLLOUT) {
        if (dst_client_msg != nullptr) {
          /*
            发送报头
          */
          ssize_t n = SendData(dst_client_msg, (char *)&cur_client_msg->header,
                               sizeof(HeaderInfo), cur_client);
          if (n <= 0) {
            continue;
          }
          /*
            发送报文
          */
          n = SendData(dst_client_msg, cur_client_msg->buffer,
                       cur_client_msg->header.len, cur_client);
          if (n <= 0) {
            continue;
          }
        }
      }
    }
  }
  return 0;
}

void EpollEvent::CloseAllFd() {
  std::cout << "Close all fd" << std::endl;
  if (epoll_sockfd_ != -1) {
    close(epoll_sockfd_);
    DelFd(epoll_fd_, epoll_sockfd_);
    logger_->Log(Logger::INFO,
                 "EpollEvent:\tsend FIN and stop listening on port %d\n",
                 epoll_port_);
  }
  for (auto const &client : client_map_) {
    if (client.second->header.connfd != -1) {
      shutdown(client.second->header.connfd, SHUT_WR);
    }
  }
}

void EpollEvent::ExitEpoll() {
  logger_->Log(Logger::INFO, "EpollEvent:\treceived SIGINT signal\n");
  CloseAllFd();
  if (client_map_.empty()) {
    logger_->Log(Logger::INFO, "EpollEvent:\tAll clients are not connected\n");
  }
}

sigfunc *EpollEvent::Signal(int signo, sigfunc *func) {
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(signo, &act, &oact) < 0) {
    return SIG_ERR;
  }
  return oact.sa_handler;
}

void EpollEvent::SigIntHandler(int sig) {
  std::cout << "Caught SIGINT" << std::endl;
  exit_flag_ = 1;
}