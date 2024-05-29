#include "RelayServer.h"

#include <asm-generic/errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <iostream>

#include "Connection.h"
#include "Logger.h"
#include "NetAddress.h"
#include "Socket.h"
#include "Utils.h"

int RelayServer::m_exit_flag_ = 0;

sigfunc* RelayServer::Signal(int signo, sigfunc* func) {
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
  m_exit_flag_ = 1;
}

RelayServer::RelayServer(const std::string& ipaddr, const int port)
    : m_serv_addr_(NetAddress(ipaddr.c_str(), port)), m_listener_(NSocket()) {
  m_log_ = new Logger("server.log");
  m_epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  m_listener_.Bind(m_serv_addr_, true);  // 设置重用IP地址和端口
  Signal(SIGINT, SigIntHandler);
  // 这里可能出现BROKEN
  // PIPE问题，即在client关闭后仍向connection发送数据，这里我们仅仅简单地忽略该信号
  Signal(SIGPIPE, SIG_IGN);
}

RelayServer::~RelayServer() {
  if (m_epoll_fd_ != -1) {
    close(m_epoll_fd_);
  }
  for (auto const& conn : m_fd_conn_) {
    if (conn.second != nullptr) {
      delete conn.second;  // 调用Connection析构函数
    }
  }
  // 显示释放LOG
  delete m_log_;
}

void RelayServer::Start() {
  m_listener_.Listen();  // 监听
  m_log_->Log(Logger::INFO, "RelayServer:\tListen on %s:%d",
              m_serv_addr_.GetIp(), m_serv_addr_.GetPort());
  SetEvent(m_listener_.GetFd(), ADD_EVENT, EPOLLIN);
  EventsLoop();
}

// void RelayServer::CloseAllFd(){}

void RelayServer::SetEvent(int fd, int flag, int events) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  if (flag) {
    ev.events = events;
    ev.data.fd = fd;
    int ret = epoll_ctl(m_epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    assert(ret != -1);
  } else {
    ev.events = events;
    ev.data.fd = fd;
    int ret = epoll_ctl(m_epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    assert(ret != -1);
  }
}

void RelayServer::DelEvent(int fd) {
  epoll_ctl(m_epoll_fd_, EPOLL_CTL_DEL, fd, NULL);
}

void RelayServer::EventsLoop() {
  while (1) {
    int nfds = epoll_wait(m_epoll_fd_, m_events_, MAXEVENTS, -1);  // 不设置超时
    if (nfds < 0) {
      // 处理中断
      if (m_exit_flag_) {
        break;
      }
      m_log_->Log(Logger::ERROR,
                  "RelayServer:\tEPOLL WAIT ERROR: no ready fd\n");
      // LOG_ERROR("RelayServer:\tEPOLL WAIT ERROR: no ready fd\n");
    }
    for (int idx = 0; idx < nfds; ++idx) {
      // EventsHandler(i);
      int connfd = m_events_[idx].data.fd;
      if (connfd == m_listener_.GetFd()) {  // 第一次建立连接
        struct sockaddr_in clientaddr;
        NetAddress client(clientaddr);
        Connection* conn = new Connection(m_listener_.Accept(client));
        AddNewConn(conn);
      } else {
        // 处理已连接套接字的EPOLLIN和EPOLLOUT
        if (m_events_[idx].events & EPOLLIN) {
          if (m_fd_conn_.find(connfd) != m_fd_conn_.end()) {
            Connection* conn = m_fd_conn_[connfd];
            conn->GetReadcb()();
          }
        }
        if (m_events_[idx].events & EPOLLOUT) {
          if (m_fd_conn_.find(connfd) != m_fd_conn_.end()) {
            Connection* conn = m_fd_conn_[connfd];

            if (m_id_conn_.find(conn->GetdstId()) !=
                m_id_conn_.end())  // 存在对端直接发送
              conn->GetWritecb()();
          }
        }
      }
    }
  }
}

int RelayServer::AddNewConn(Connection* client) {
  /*
    更新连接参数
  */
  // 计算自己和对端的客户端ID
  client->SetCliId(m_next_client_id_);
  client->SetdstId(GetDstId(m_next_client_id_));  // 设置会话的对端
  // 设置连接状态
  client->SetState(_PKG_HD_INIT);
  client->SetShutWR(false);
  // 设置回调
  client->SetReadcb(std::bind(&RelayServer::OnRecv, this, client));
  client->SetWritecb(std::bind(&RelayServer::OnSend, this, client));

  m_id_conn_[m_next_client_id_] = client;
  m_fd_conn_[client->GetFd()] = client;
  UpdateCliId();  // 作INC操作
  SetEvent(client->GetFd(), ADD_EVENT, EPOLLIN | EPOLLOUT);
  if (m_next_client_id_ % 1000 == 0)
    printf("RelayServer:\tClients num: %ld, sockfd: %d\n", m_id_conn_.size(),
           client->GetFd());

  return 0;
}

int RelayServer::RemoveConn(const int& connfd) {
  DelEvent(connfd);
  assert(m_fd_conn_.find(connfd) != m_fd_conn_.end());
  int cliId = m_fd_conn_[connfd]->GetCliId();
  delete m_fd_conn_[connfd];
  m_fd_conn_.erase(connfd);
  m_id_conn_.erase(cliId);
  printf("RelayServer:\tclient %d is removed, Clients num %ld\n", cliId,
         m_id_conn_.size());

  m_log_->Log(Logger::INFO,
              "RelayServer:\tClient %d is removed, Clients num %d\n", cliId,
              m_id_conn_.size());

  return 0;
}

/*
  我们提供的回调函数
*/
void RelayServer::OnRecv(Connection* conn) {
  // LT模式下不断接收数据
  size_t nrecv = 0;  // 收到的字节数
  nrecv = recv(conn->GetFd(), conn->GetRecvBuf() + conn->GetRecvLen(),
               BUFFERSZ - conn->GetRecvLen(), 0);

  if (nrecv > 0) {
    do {
      if (static_cast<int>(nrecv) >= conn->GetUnRecvLen()) {
        if (conn->GetState() == _PKG_HD_INIT) {
          // recving header
          conn->SetState(_PKG_BD_INIT);
          memcpy(conn->GetHeadBuf() + HEADERSZ - conn->GetUnRecvLen(),
                 conn->GetRecvBuf() + conn->GetRecvLen(), conn->GetUnRecvLen());
          auto pair = ParseHeader((struct HeaderInfo*)conn->GetHeadBuf());
          uint32_t msglen = pair.first;
          // uint32_t dstId = pair.second; // use in the future
          if (conn->GetPacketLen() == 0) conn->SetPacketLen(msglen);
          nrecv -= conn->GetUnRecvLen();
          conn->AddRecvLen(conn->GetUnRecvLen());
          conn->SetUnRecvLen(msglen);
        } else {
          /*
            接收报文体
          */
          nrecv -= conn->GetUnRecvLen();
          conn->AddRecvLen(conn->GetUnRecvLen());
          conn->SetUnRecvLen(HEADERSZ);
          conn->SetState(_PKG_HD_INIT);
          // m_log_->Log(
          //     Logger::INFO,
          //     "RelayServer:\tServer recv whole packet from Client %d : len "
          //     "%d\n",
          //     conn->GetCliId(), conn->GetPacketLen());
          // printf(
          //     "RelayServer:\tServer recv packet from Client %d : len "
          //     "%ld\n",
          //     conn->GetCliId(), conn->GetPacketLen() + HEADERSZ);
        }
      } else {
        /*
          部分接收
        */
        if (nrecv > 0) {
          if (conn->GetState() == _PKG_HD_INIT) {
            memcpy(conn->GetHeadBuf() + HEADERSZ - conn->GetUnRecvLen(),
                   conn->GetRecvBuf() + conn->GetRecvLen(), nrecv);
          }
          conn->AddRecvLen(nrecv);
        }
        break;
      }

    } while (1);
    // 只要收到数据就设置可写事件，对dstId发送
    if (m_id_conn_.find(conn->GetdstId()) == m_id_conn_.end()) {
      // 暂时对无对端情况不处理
      return;
    }
    Connection* dstconn = m_id_conn_[conn->GetdstId()];
    this->SetEvent(dstconn->GetFd(), MOD_EVENT, EPOLLIN | EPOLLOUT);

  } else if (nrecv == 0) {
    // 关闭当前connection的fd
    conn->SetShutWR(true);

    shutdown(conn->GetFd(), SHUT_WR);
    RemoveConn(conn->GetFd());
    return;
  } else {
    // nrecv == -1
    if (errno != EWOULDBLOCK) {
      RemoveConn(conn->GetFd());
      m_log_->Log(Logger::ERROR,
                  "RelayServer: client %d connfd %d recv error\n",
                  conn->GetCliId(), conn->GetFd());
      exit(1);
    } else if (errno == EINTR) {
      return;
    } else {
      // EWOULDBLOCK
      m_log_->Log(Logger::WARNING,
                  "RelayServer: client %d connfd %d recv EWOULDBLOCK\n",
                  conn->GetCliId(), conn->GetFd());
    }
  }
  // return nrecv;
}

void RelayServer::OnSend(Connection* dstconn) {
  size_t nsend = 0;
  if (m_id_conn_.find(dstconn->GetdstId()) == m_id_conn_.end()) {
    // 暂时对无对端情况不处理
    return;
  }
  if (dstconn->GetShutWR() == true) {
    return;
  }
  Connection* conn = m_id_conn_[dstconn->GetdstId()];
  nsend = send(dstconn->GetFd(), conn->GetRecvBuf(), conn->GetRecvLen(), 0);
  if (nsend > 0) {
    // 发送成功
    memcpy(conn->GetRecvBuf(), conn->GetRecvBuf() + nsend,
           conn->GetRecvLen() - nsend);
    conn->SubRecvLen(nsend);
    // printf(
    //     "RelayServer:\tServer send packet to Client %d : len "
    //     "%ld\n",
    //     dstconn->GetCliId(), nsend);
    // if (conn->GetRecvLen() == 0)
    //   this->SetEvent(dstconn->GetFd(), MOD_EVENT, EPOLLIN);

  } else if (nsend == 0) {
    return;
  } else if (errno == EINTR) {
    return;
  } else {
    // nsend == -1
    if (errno != EWOULDBLOCK) {
      RemoveConn(conn->GetFd());
      m_log_->Log(Logger::ERROR,
                  "RelayServer: client %d connfd %d send error\n",
                  conn->GetCliId(), conn->GetFd());
      exit(1);
    } else {
      // EWOULDBLOCK
      m_log_->Log(Logger::WARNING,
                  "RelayServer: client %d connfd %d send EWOULDBLOCK\n",
                  conn->GetCliId(), conn->GetFd());
    }
  }
  // return nsend;
}