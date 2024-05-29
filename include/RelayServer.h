#ifndef RELAYSERVER_H
#define RELAYSERVER_H
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "Connection.h"
#include "Logger.h"
#include "NetAddress.h"
#include "Socket.h"
#include "Utils.h"

typedef void sigfunc(int);  // 信号处理函数
class RelayServer {
 public:
  // friend class Connection;
  RelayServer(const std::string& ipaddr = "127.0.0.1", const int port = 1234);
  ~RelayServer();
  /**
   * @brief 开启RelayServer
   *
   */
  void Start();

 private:
  /**
   * @brief Set the Event object
   *
   * @note flag为1是EPOLL_CTL_ADD，flag为0是EPOLL_CTL_MOD；events是监听事件
   * @param fd
   * @param flag
   * @param events
   */
  void SetEvent(int fd, int flag, int events);
  /**
   * @brief Delete event in epoll
   *
   * @param fd
   */
  void DelEvent(int fd);
  /**
   * @brief 事件循环
   *
   * @note
   * while无限循环，使用epoll_wait;listenfd监听EPOLLIN事件，Socket->Accept()
   *       clientfd监听EPOLLIN事件(recv and send)；如果未发送完监听EPOLLOUT事件
   */
  void EventsLoop();
  /**
   * @brief 更新client ID
   *
   */
  void UpdateCliId() { m_next_client_id_++; }
  /**
   * @brief 向映射中增加新的Connection
   *
   * @param client
   * @return int
   */
  int AddNewConn(Connection* client);
  /**
   * @brief 向映射中移除新的Connection
   *
   * @param connfd
   * @return int
   */
  int RemoveConn(const int& connfd);
  /**
   * @brief clientfd上EPOLLIN事件的回调
   *
   * @param conn
   */
  void OnRecv(Connection* conn);
  /**
   * @brief clientfd上EPOLLOUT事件的回调
   *
   * @param conn
   */
  void OnSend(Connection* conn);
  /**
   * @brief 退出或销毁对象时，调用的清理函数
   *
   */
  // void CloseAllFd();
  /**
   * @brief 对signo信号的处理函数
   *
   * @param signo
   * @param func
   * @return sigfunc*
   */
  sigfunc* Signal(int signo, sigfunc* func);
  static void SigIntHandler(int sig);

 private:
  NetAddress m_serv_addr_;                  // 服务器地址
  NSocket m_listener_;                      // 监听套接字
  int m_epoll_fd_;                          // epoll创建的描述符
  struct epoll_event m_events_[MAXEVENTS];  // epoll监听描述符集合

  size_t m_next_client_id_{0};            // 下一个客户端ID
  std::map<int, Connection*> m_id_conn_;  // id与connection的映射
  std::map<int, Connection*> m_fd_conn_;  // fd与connection的映射
  /*
    信号函数使用
  */
  static int m_exit_flag_;  // 退出标志
  enum { MOD_EVENT, ADD_EVENT };
  /*
    日志
  */
  Logger* m_log_;
};

#endif /* RELAYSERVER_H_ */