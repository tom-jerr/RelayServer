
#include "../include/Server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>

#include "../include/Logger.h"
#include "../include/Utils.h"

int RelayServer::exit_flag_ = 0;  // 需要在外部进行声明-zx

RelayServer::RelayServer()
    : epoll_sockfd_(-1), epoll_fd_(-1), temp_file_(false) {
  Signal(SIGINT, SigIntHandler);
  logger_ = new Logger("server.log");
}

RelayServer::RelayServer(bool support_tmpfile)
    : epoll_sockfd_(-1), epoll_fd_(-1), temp_file_(true) {
  Signal(SIGINT, SigIntHandler);
  logger_ = new Logger("server.log");
}

RelayServer::~RelayServer() {
  logger_->Log(Logger::INFO, "RelayServer is destructed\n");
  int ret = 1;
  if (epoll_sockfd_ != -1) {
    close(epoll_sockfd_);
    ASSERT(ret == 1);
  }
  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

void RelayServer::UpdateNextClientId() { next_client_id_++; }

int RelayServer::AddNewClient(MessageInfo *clientInfo) {
  clientInfo->header.cliId = next_client_id_;
  client_id_map_[next_client_id_] = clientInfo;
  client_map_[clientInfo->connfd] = clientInfo;
  UpdateNextClientId();
  AddFd(epoll_fd_, clientInfo->connfd, 1, 0);
  logger_->Log(Logger::INFO,
               "RelayServer:\tClient %d is added, fd is %d\t(%d in total)\n",
               clientInfo->header.cliId, clientInfo->connfd,
               client_map_.size());
  return 0;
}

int RelayServer::RemoveClient(const int &connfd) {
  ASSERT(client_map_.find(connfd) != client_map_.end());
  size_t cliId = client_map_[connfd]->header.cliId;
  client_map_.erase(connfd);
  client_id_map_.erase(cliId);
  if (cliId < next_client_id_) {
    next_client_id_ = cliId;
  }
  int ret = close(connfd);
  ASSERT(ret >= 0);
  DelFd(epoll_fd_, connfd);
  logger_->Log(Logger::INFO,
               "RelayServer:\tClient %d is removed, fd is %d\t(%d in total)\n",
               cliId, connfd, client_map_.size());
  return 0;
}

void RelayServer::WriteMsg2File(const size_t &id,
                                const std::string &file_name) {
  FILE *fp = fopen(file_name.c_str(), "a+");
  if (fp == nullptr) {
    logger_->Log(Logger::ERROR,
                 "RelayServer:\tOPEN FILE ERROR: client %d open file error\n",
                 id);
    return;
  }
  // 向文件中写入数据
  fwrite(client_map_[id]->buffer, sizeof(char), client_map_[id]->header.len - 1,
         fp);
  // 将文件偏移移到开头
  fseek(fp, 0, SEEK_SET);
  client_file_map_[id] = fp;
}

char *RelayServer::ReadMsgFromFile(const size_t &id) {
  FILE *fp = client_file_map_[id];
  if (fp == nullptr) {
    logger_->Log(Logger::ERROR,
                 "RelayServer:\tNO FILE ERROR: client %d has no file error\n",
                 id);
    return nullptr;
  }
  // 将文件偏移移到开头
  fseek(fp, 0, SEEK_SET);
  // 读取文件
  char *buffer = new char[client_map_[id]->header.len];
  fread(buffer, sizeof(char), client_map_[id]->header.len - 1, fp);
  return buffer;
}

int RelayServer::RecvData(MessageInfo *msg, const size_t &id,
                          const size_t &fd) {
  // 获取头部和连接描述符
  // HeaderInfo *header = &msg->header;
  char *buffer = msg->buffer;
  // int connfd = msg->connfd;
  /*
    非阻塞接收数据
    最多接收字节长度：BUFFERSZ-已经接收到的数据长度
  */
  int ret = recv(fd, buffer + msg->recvlen, BUFFERSZ - msg->recvlen, 0);
  if (ret < 0) {
    if (errno != EWOULDBLOCK) {
      logger_->Log(Logger::ERROR,
                   "RECV ERROR: RelayServer:\tclient %d recv error\n", id);
      return -1;
    }
    /*
      记录EWOULDBLOCK事件
    */
    logger_->Log(
        Logger::ERROR,
        "RelayServer:\tEWOULDBLOCK ERROR: client %d recv EWOULDBLOCK\n", id);
  } else if (ret == 0) {
    /*
      客户端关闭连接，不再向套接字中写入数据
    */
    if (msg->shutwr == 0) {
      msg->shutwr = 1;
      shutdown(fd, SHUT_WR);  // 发送FIN包
    } else {
      shutdown(fd, SHUT_RD);  // 发送FIN包
    }
    RemoveClient(fd);

    return 0;
  } else {
    /*
      接收报文：头部+数据
    */
    for (;;) {
      if (ret >= msg->unrecvlen) {
        if (!msg->head_recv) {
          /*
            接收头部，将未接收的报文长度设置为报文长度
          */
          msg->head_recv = true;
          // 解析头部数据，获取报文体长度
          uint32_t header_cliID = msg->header.cliId;
          memcpy(&msg->header + HEADERSZ - msg->unrecvlen,
                 msg->buffer + msg->recvlen, ret);
          auto res = ParseHeader(&msg->header, id);
          uint16_t connetlen = res.first;
          msg->header.cliId = header_cliID;
          msg->header.len = connetlen;
          logger_->Log(
              Logger::INFO,
              "RelayServer:\tClient %d recv header: len %d, cliId %d\n", id,
              connetlen, header_cliID);

          // client_id_map_[id]->cliId = header_cliID;
          ret = ret - msg->unrecvlen;
          msg->recvlen += msg->unrecvlen;
          msg->unrecvlen = connetlen;
        } else {
          /*
            接收报文体，将未接收的报文长度变为头部大小
          */
          ret = ret - msg->unrecvlen;
          msg->recvlen += msg->unrecvlen;
          msg->unrecvlen = HEADERSZ;
          msg->head_recv = false;
        }
      } else {
        /*
          部分接收
        */
        if (!msg->head_recv) {
          memcpy(&msg->header + HEADERSZ - msg->unrecvlen,
                 msg->buffer + msg->recvlen, ret);
        }
        msg->recvlen += ret;
        msg->unrecvlen -= ret;
        break;
      }
    }
  }
  return ret;
}

int RelayServer::SendData(MessageInfo *msg, const size_t &id,
                          const size_t &fd) {
  // 将src中的信息发送到dst客户端
  // int connfd = dstmsg->connfd;

  int ret = send(fd, msg->buffer, msg->recvlen, 0);
  if (ret < 0) {
    if (errno != EWOULDBLOCK) {
      RemoveClient(fd);
      logger_->Log(Logger::ERROR,
                   "RelayServer:\tSEND ERROR: server send to client %d error\n",
                   id);
      return -1;
    } else {
      logger_->Log(Logger::ERROR,
                   "RelayServer:\tNO SPACE ERROR: server has no spacesend to "
                   "client %d error\n",
                   id);
    }
  } else if (ret >= 0) {
    /*
      发送成功，将发送的报文从缓冲区中删除，将整个缓冲区前移
    */
    memcpy(msg->buffer, msg->buffer + ret, msg->recvlen - ret);
    msg->recvlen -= ret;
  }
  return ret;
}

void RelayServer::StartServer(const std::string &ipaddr, const int port) {
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
  /*
    设置端口复用
  */
  int reuse = 1;
  setsockopt(epoll_sockfd_, SOL_SOCKET, SO_REUSEPORT, (const void *)&reuse,
             sizeof(int));

  servaddr_.sin_family = AF_INET;
  InetPton(AF_INET, ipaddr.c_str(), (void *)&servaddr_.sin_addr);
  // std::cout << "inet_pton:\t" << ret << std::endl;
  servaddr_.sin_port = htons(epoll_port_);
  Bind(epoll_sockfd_, (struct sockaddr *)&servaddr_, sizeof(servaddr_));
  logger_->Log(Logger::INFO, "RelayServer:\tBind to %s:%d, fd: %d\n",
               ipaddr.c_str(), epoll_port_, epoll_sockfd_);
  Listen(epoll_sockfd_, BACKLOG);
  logger_->Log(Logger::INFO, "RelayServer:\tListen on port %s:%d\n",
               ipaddr.c_str(), epoll_port_);
  /*
    创建epoll_fd
  */
  epoll_fd_ = epoll_create(MAXEVENTS);
  ASSERT(epoll_fd_ != -1);

  /*
    将epoll_sockfd_添加到epoll_fd_中，均使用LT模式
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
      // INT信号中断
      if (exit_flag_) {
        ExitEpoll();
        break;
      }
      logger_->Log(Logger::ERROR,
                   "RelayServer:\tEPOLL WAIT ERROR: exit with error\n");
      CloseAllFd();
    }
    if (EventsHandler(events, nfds) < 0) {
      logger_->Log(Logger::ERROR,
                   "RelayServer:\tEPOLL HANDLER ERROR: exit with error\n");
      CloseAllFd();
    }
    if (exit_flag_) {
      ExitEpoll();
      break;
    }
  }
}

int RelayServer::EventsHandler(struct epoll_event *events, const int &nready) {
  for (int i = 0; i < nready; ++i) {
    int sockfd = events[i].data.fd;
    if (sockfd == epoll_sockfd_) {
      /*
        有新的连接
      */
      struct sockaddr_in clientaddr;
      socklen_t len = sizeof(clientaddr);

      // 非阻塞accept，直到connfd返回-1才结束
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
                         "RelayServer:\tACCEPT ERROR: exit with error\n");
            return -1;
          }
        }
        /*
          声明一个新的客户端，连接描述符为connfd
        */
        MessageInfo *client = new MessageInfo;
        client->connfd = connfd;
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
      if (client_id_map_.find(dst_client) != client_id_map_.end()) {
        dst_client_msg = client_id_map_[dst_client];
      }

      if (dst_client_msg == nullptr) {
        /*
          目的客户端不存在
        */
        // 如果支持暂存，将报文写入文件
        // if (this->temp_file_) {
        //   std::cout << "Save msg to server\n";
        //   std::string file_name = std::to_string(cur_client) + "_file";
        //   WriteMsg2File(cur_client, file_name);
        // }
        // logger_->Log(Logger::INFO, "INFO: dst client is not connected\n");
      }

      /*
        Epoll有可读事件，读取报文到缓冲区
      */
      if (events[i].events & EPOLLIN) {
        ssize_t n = RecvData(cur_client_msg, cur_client, sockfd);
        if (n <= 0) {
          continue;
        }
      }
      /*
        Epoll有可写事件
      */
      if ((events[i].events & EPOLLOUT) && cur_client_msg->shutwr != 1) {
        if (dst_client_msg != nullptr) {
          /*
            发送报文
          */
          ssize_t n;
          n = SendData(dst_client_msg, dst_client, sockfd);
          if (n <= 0) {
            continue;
          }
        }
      }
    }
  }
  return 0;
}

void RelayServer::CloseAllFd() {
  std::cout << "Close all fd" << std::endl;
  if (epoll_sockfd_ != -1) {
    close(epoll_sockfd_);
    DelFd(epoll_fd_, epoll_sockfd_);
    logger_->Log(Logger::INFO,
                 "RelayServer:\tsend FIN and stop listening on port %d\n",
                 epoll_port_);
  }
  for (auto const &client : client_map_) {
    if (static_cast<int>(client.second->connfd) != -1) {
      if (client.second->shutwr == 0) {
        shutdown(client.second->connfd, SHUT_WR);
        client.second->shutwr = 1;
      }
    }
  }
}

void RelayServer::ExitEpoll() {
  logger_->Log(Logger::INFO, "RelayServer:\treceived SIGINT signal\n");
  CloseAllFd();
  if (client_map_.empty()) {
    logger_->Log(Logger::INFO, "RelayServer:\tAll clients are not connected\n");
  }
}

sigfunc *RelayServer::Signal(int signo, sigfunc *func) {
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(signo, &act, &oact) < 0) {
    return SIG_ERR;
  }
  return oact.sa_handler;
}

void RelayServer::SigIntHandler(int sig) {
  std::cout << "Caught SIGINT" << std::endl;
  exit_flag_ = 1;
}