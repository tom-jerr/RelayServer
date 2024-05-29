#include "StressGenerator.h"

#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <bits/types/struct_timespec.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "Logger.h"
#include "Utils.h"
#define CONN_SIZE 10
#define ERROR_MAX 10
#define WAIT_CONN_MAX 200

int StressGenerator::exit_flag_ = 0;

StressGenerator::StressGenerator() {
  logger_ = new Logger("StressGenerator.log");
  Signal(SIGINT, SigIntHandler);
}
StressGenerator::~StressGenerator() {
  for (auto &client : clients_) {
    if (client.second.buffer != nullptr) {
      delete client.second.buffer;
    }
  }
  delete logger_;

  if (packet_ != nullptr) {
    delete[] packet_;
  }
}

void StressGenerator::GeneratePacket() {
  ASSERT(packet_ == nullptr);
  packet_ = new char[payloadSize_];
  memset(packet_, 'a', payloadSize_);
  logger_->Log(Logger::INFO, "StressGenerator - Generate %zd bytes Packet",
               payloadSize_);
  printf("StressGenerator - Generate %zd bytes Packet", payloadSize_);
}

void StressGenerator::AddClient(int sockfd, int state) {
  ASSERT(state == 0 || state == -1);
  ASSERT(clients_.find(sockfd) == clients_.end());
  ClientInfo client;
  client.connfd = sockfd;
  client.state = state;
  if (state == 0) {
    client.buffer = new ClientBuffer;
    client.buffer->sendpackets = this->packet_;
    ++connNum_;
  } else {
    ++unconnNum_;
  }
  clients_[sockfd] = client;
  AddFd(epollfd_, sockfd, 1, 0);
  // AddOutFd(epollfd_, sockfd);
}

int StressGenerator::AddDoubleClients() {
  int errorTimes = ERROR_MAX;
  int connTimes = CONN_SIZE;
  while (static_cast<int>(clients_.size()) < cliCount_ && connTimes > 0) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      logger_->Log(Logger::ERROR, "StressGenerator - socket error");
      errorTimes--;
      if (errorTimes < 0) {
        return -1;
      }
      continue;
    }
    SetNonBlocking(sockfd);
    errno = 0;
    int ret = 0;
    /*
      Nonblocking connect
    */
    if ((ret = connect(sockfd, (struct sockaddr *)&servaddr_,
                       sizeof(servaddr_))) < 0) {
      if (errno != EINPROGRESS) {
        logger_->Log(Logger::ERROR, "StressGenerator - sockfd %d connect error",
                     sockfd);
        close(sockfd);
        errorTimes--;
        if (errorTimes < 0) {
          return -1;
        }
        continue;
      } else {
        AddClient(sockfd, -1);
      }
    }
    /* 直接连接建立 */
    else {
      AddClient(sockfd, 0);
      // logger_->Log(Logger::INFO,
      //              "StressGenerator - client connfd %d - [connnum] %d "
      //              "[unconnnum] %d [allnum] %d",
      //              sockfd, connNum_, unconnNum_, connNum_ + unconnNum_);
    }
    connTimes--;
  }
  return 0;
}

int StressGenerator::RemoveClient(int sockfd) {
  ASSERT(clients_.find(sockfd) != clients_.end());
  if (clients_[sockfd].state == 0) {
    --connNum_;
  } else {
    --unconnNum_;
  }
  if (clients_[sockfd].buffer != nullptr) {
    delete clients_[sockfd].buffer;
  }
  clients_.erase(sockfd);
  Close(sockfd);
  DelFd(epollfd_, sockfd);
  // LOG_INFO(
  //     "StressGenerator - Remove client connfd %d - [connnum] %d [unconnnum] "
  //     "%d [allnum] %d",
  //     sockfd, connNum_, unconnNum_, connNum_ + unconnNum_);
  printf("StressGenerator - Remove client connfd %d - [allnum] %d ", sockfd,
         connNum_ + unconnNum_);
  return 0;
}

void StressGenerator::CloseAllClients() {
  shutdown_flag_ = 1;
  logger_->Log(Logger::INFO, "StressGenerator - Close all clients");
  printf("StressGenerator - Close all clients");
  for (auto &cli : clients_) {
    if (cli.second.state == 0) {
      shutdown(cli.first, SHUT_WR);
      cli.second.state = 1;
    }
  }
}

void StressGenerator::CalcDelay(struct timespec *timestamp) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  if (timestamp->tv_sec > now.tv_sec ||
      (timestamp->tv_sec == now.tv_sec && timestamp->tv_nsec > now.tv_nsec)) {
    return;
  }
  g_totalDelay += static_cast<double>(now.tv_sec - timestamp->tv_sec) * 1000 +
                  static_cast<double>(now.tv_nsec - timestamp->tv_nsec) / 1e6;
}

uint16_t StressGenerator::HandleHeaderWithTime(HeaderInfo *header,
                                               const size_t &fd) {
  uint32_t msglen = ntohl(header->len);
  struct timespec timestamp;
  timestamp.tv_sec = ntoh64(header->sec);
  timestamp.tv_nsec = ntoh64(header->nsec);
  CalcDelay(&timestamp);
  return msglen;
}

// TODO: 考虑使用信号量进行消息异步的流量控制，这里还是同步消息
int StressGenerator::EventsHandler(struct epoll_event *events,
                                   const int &ready) {
  for (int i = 0; i < ready; ++i) {
    int sockfd = events[i].data.fd;
    ASSERT(clients_.find(sockfd) != clients_.end());
    // 如果是未连接套接字
    if (clients_[sockfd].state == -1) {
      int error;
      socklen_t len = sizeof(error);
      if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
        errno = 0;
        logger_->Log(Logger::ERROR,
                     "StressGenerator - client %d - connect error %s", sockfd,
                     strerror(error));
        RemoveClient(sockfd);
        continue;
      }
      clients_[sockfd].state = 0; /* 设置状态为已连接(等待接收头部) */
      clients_[sockfd].buffer = new ClientBuffer; /* 分配缓冲区 */
      clients_[sockfd].buffer->sendpackets = this->packet_;
      ++connNum_;
      --unconnNum_;
      // LOG_INFO("StressGenerator - client %d - connected", sockfd);
      if (recordFlag_ == 0 && connNum_ >= static_cast<int>(cliCount_)) {
        recordFlag_ = 1;
        // LOG_INFO(
        //     "StressGenerator - %zd connected clients, start "
        //     "to send packets",
        //     connNum_);
      }
    }

    // 已连接套接字
    ASSERT(clients_[sockfd].state != -1);
    ASSERT(clients_[sockfd].buffer != nullptr);
    ASSERT(clients_[sockfd].buffer->sendpackets != nullptr);
    ClientBuffer *buffer = clients_[sockfd].buffer;

    /*
      接收服务器传回的信息
    */
    if ((events[i].events & EPOLLIN)) {
      while (true) {
        buffer->recvlen = 0;
        ssize_t ret = recv(sockfd, buffer->buffer, BUFFERSZ, 0);
        if (ret > 0) {
          while (true) {
            // 接收全部的报头或者报文
            if (ret >= buffer->unrecvlen) {
              // 处理报头
              if (!buffer->head_recv) {
                memcpy(&buffer->recv + HEADERSZ - buffer->unrecvlen,
                       buffer->buffer + buffer->recvlen, buffer->unrecvlen);
                size_t msglen = HandleHeaderWithTime(&buffer->recv, sockfd);
                g_recvPackets++;
                ret = ret - buffer->unrecvlen;
                buffer->recvlen += buffer->unrecvlen;
                buffer->unrecvlen = msglen;
                buffer->head_recv = true;
              } else {
                ret -= buffer->unrecvlen;
                buffer->recvlen += buffer->unrecvlen;
                buffer->head_recv = false;
                buffer->unrecvlen = HEADERSZ;
                // printf(
                //     "StressGenerator - client %d - recv "
                //     "packet %d bytes\n",
                //     sockfd, buffer->recvlen);
                buffer->recvlen = 0;
                clients_[sockfd].state = 2;  // 设置状态为已读取报文
                break;
              }
            }
            // 部分接收
            else {
              if (ret > 0) {
                if (!buffer->head_recv) {
                  memcpy(&buffer->recv + HEADERSZ - buffer->unrecvlen,
                         buffer->buffer + buffer->recvlen, ret);
                }
                buffer->unrecvlen -= ret;
                buffer->recvlen += ret;
              }
              break;
            }
          }
        }
        // ret == 0
        else if (ret == 0) {
          if (clients_[sockfd].state == 1) {
            shutdown(sockfd, SHUT_WR);
          }
          RemoveClient(sockfd);
          break;
        } else {
          if (errno != EWOULDBLOCK) {
            logger_->Log(Logger::ERROR,
                         "StressGenerator -client %d -  recv error", sockfd);
            RemoveClient(sockfd);
          }

          break;
        }
      }
    }

    // 需要等待开始记录才能发送数据
    /*
      向服务器发送消息
    */
    if ((events[i].events & EPOLLOUT) && recordFlag_ == 1 &&
        clients_[sockfd].state != 1) {
      while (true) {
        ssize_t ret = 0;
        // 说明报文头还未发送
        if (buffer->sendlen < static_cast<int>(HEADERSZ)) {
          if (buffer->sendlen == 0) {
            GetHeader(payloadSize_, sockfd, &buffer->send);
            g_sendPackets++;
          }
          ret = send(sockfd, &buffer->send + buffer->sendlen,
                     HEADERSZ - buffer->sendlen, 0);
        }
        // 发送整个报文
        else {
          ret = send(sockfd, buffer->sendpackets + buffer->sendlen - HEADERSZ,
                     payloadSize_ - buffer->sendlen + HEADERSZ, 0);
        }

        if (ret >= 0) {
          buffer->sendlen += ret;
          if (buffer->sendlen == static_cast<int>(payloadSize_ + HEADERSZ)) {
            // printf("StressGenerator - client %d - send packet %d bytes\n",
            //        sockfd, buffer->sendlen);
            buffer->sendlen = 0;
            clients_[sockfd].state = 1;
            break;
          }
        } else {
          if (errno != EWOULDBLOCK) {
            logger_->Log(Logger::ERROR,
                         "StressGenerator - client %d - send error", sockfd);
            RemoveClient(sockfd);
          }
          break;
        }
      }
    }
  }
  return 0;
}

int StressGenerator::EventsLoop(const char *ip, const char *port) {
  // 初始化服务器地址
  bzero(&servaddr_, sizeof(servaddr_));
  servaddr_.sin_family = AF_INET;
  InetPton(AF_INET, ip, &servaddr_.sin_addr);
  servaddr_.sin_port = htons(atoi(port));
  // 创建epoll
  struct epoll_event events[MAXEVENTS];
  epollfd_ = epoll_create(1);
  ASSERT(epollfd_ >= 0);

  while (true) {
    // 添加客户端
    if (static_cast<int>(clients_.size()) < cliCount_ && shutdown_flag_ == 0) {
      if (AddDoubleClients() < 0) {
        logger_->Log(Logger::ERROR, "StressGenerator - AddDoubleClients error");
        CloseAllClients();
      }
    }
    // 等待事件
    int nready = epoll_wait(epollfd_, events, MAXEVENTS, -1);
    if (nready < 0) {
      logger_->Log(Logger::ERROR, "StressGenerator - epoll_wait error");
      CloseAllClients();
    }
    // 处理事件
    if (EventsHandler(events, nready) < 0) {
      logger_->Log(Logger::ERROR, "StressGenerator - EventsHandler error");
      CloseAllClients();
    }
    if (g_sendPackets % 1000 == 0) {
      std::cout << "StressGenerator - Send packets: " << g_sendPackets
                << " Recv packets: " << g_recvPackets << std::endl;
    }
    /*
      发送指定数量报文后结束压力测试
    */
    // if (static_cast<int>(g_sendPackets) >= 2 * cliCount_ &&
    //     static_cast<int>(g_recvPackets) >= 2 * cliCount_) {
    //   break;
    // }
    if (exit_flag_ || shutdown_flag_) {
      clock_gettime(CLOCK_REALTIME, &g_endTime);
      CloseAllClients();
      if (clients_.empty()) {
        logger_->Log(Logger::INFO,
                     "StressGenerator - all connected sockets are closed");
        return -1;
      }
      break;
    }
  }
  return 0;
}

void StressGenerator::StartStress(const char *ip, const char *port,
                                  int sessions, int packetSize) {
  // 参数检查
  if (sessions <= 0) {
    std::cout << "sessions must be greater than 0" << std::endl;
    exit(EXIT_FAILURE);
  }
  if (sessions * 2 > MAXEVENTS) {
    std::cout << "sessions must be less than " << MAXEVENTS / 2 << std::endl;
    exit(EXIT_FAILURE);
  }
  if (packetSize <= static_cast<int>(HEADERSZ)) {
    std::cout << "packetSize must be greater than " << HEADERSZ << std::endl;
    exit(EXIT_FAILURE);
  }

  this->cliCount_ = sessions * 2;
  this->payloadSize_ = packetSize - HEADERSZ;

  // 生成报文
  GeneratePacket();
  logger_->Log(Logger::INFO, "StressGenerator - StartStress");
  clock_gettime(CLOCK_REALTIME, &g_startTime);
  EventsLoop(ip, port);  // 一直进行报文的发送和接收；接收到SIGINT信号结束

  if (recordFlag_ == 0) {
    logger_->Log(Logger::ERROR,
                 "StressGenerator - no enough clients connected");
  } else {
    g_averageDelay = g_totalDelay / g_recvPackets;
    logger_->Log(Logger::INFO,
                 "StressGenerator - generator - %zd connected clients, "
                 "send %lu packets, recv %lu packets, total delay %.2f ms, "
                 "average delay %.2f ms",
                 connNum_, g_sendPackets, g_recvPackets, g_totalDelay,
                 g_averageDelay);
    logger_->Log(Logger::INFO, "StressGenerator - generator - Test time %.6f s",
                 (g_endTime.tv_sec - g_startTime.tv_sec) +
                     (g_endTime.tv_nsec - g_startTime.tv_nsec) / 1e9);
    printf(
        "StressGenerator - generator - %d connected clients, "
        "send %lu packets, recv %lu packets, total delay %.2f ms, "
        "average delay %.2f ms\n",
        connNum_, g_sendPackets, g_recvPackets, g_totalDelay, g_averageDelay);
    printf("StressGenerator - generator - Test time %.6f s\n",
           (g_endTime.tv_sec - g_startTime.tv_sec) +
               (g_endTime.tv_nsec - g_startTime.tv_nsec) / 1e9);
  }
  return;
}

sigfunc *StressGenerator::Signal(int signo, sigfunc *func) {
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(signo, &act, &oact) < 0) {
    return SIG_ERR;
  }
  return oact.sa_handler;
}

void StressGenerator::SigIntHandler(int sig) {
  std::cout << "Caught SIGINT" << std::endl;
  exit_flag_ = 1;
}