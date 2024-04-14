#ifndef SERVER_H_
#define SERVER_H_
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <map>
#include <string>

#include "Logger.h"
#include "Utils.h"

typedef void sigfunc(int);  // 信号处理函数
/*
  Epoll event封装
*/
class RelayServer {
 private:
  struct sockaddr_in servaddr_;  // epoll服务器地址
  // struct epoll_event epoll_;                // epoll事件handler
  int epoll_port_;                          // 服务器端口
  int epoll_sockfd_;                        // epoll接收连接fd
  int epoll_fd_;                            // epoll创建的fd
  static int exit_flag_;                    // 退出标志
  size_t next_client_id_ = 0;               // 下一个客户端ID
  bool temp_file_;                          // 服务器暂存客户端信息
  std::map<int, MessageInfo*> client_map_;  // 客户端connfd与MessageInfo的映射
  std::map<int, MessageInfo*> client_id_map_;  // 客户端ID与MessageInfo的映射
  std::map<int, FILE*> client_file_map_;  // 客户端暂存信息文件指针
  Logger* logger_;                        // 日志

 public:
  /*
    Constructor and Destructor
  */
  RelayServer();
  RelayServer(bool support_tmpfile);
  ~RelayServer();

  /*
    初始化server并启动
  */
  void StartServer(const std::string& ipaddr = "127.0.0.1",
                   const int port = 1234);
  /*
    处理epoll的事件
  */
  int EventsHandler(struct epoll_event* events, const int& nready);
  /*
    更新下一个客户端编号
  */
  void UpdateNextClientId();
  /*
    加入新客户端
  */
  int AddNewClient(MessageInfo* client);
  /*
    移除指定客户端
  */
  int RemoveClient(const int& connfd);
  /*
    发送数据
  */
  int SendData(MessageInfo* msg, const size_t& id, const size_t& fd);
  /*
    接收数据
  */
  int RecvData(MessageInfo* msg, const size_t& id, const size_t& fd);
  /*
    向文件中写入接收到的数据
  */
  void WriteMsg2File(const size_t& id, const std::string& file_name);
  /*
    从文件中读取数据
  */
  char* ReadMsgFromFile(const size_t& id);
  /*
    关闭全部连接
  */
  void CloseAllFd();
  /*
    退出Epoll
  */
  void ExitEpoll();
  /*
    signal handler
  */
  sigfunc* Signal(int signo, sigfunc* func);
  /*
    处理INT信号
  */
  static void SigIntHandler(int sig);
};

#endif  // SERVER_H_