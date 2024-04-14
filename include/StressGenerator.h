#ifndef STRESSGENEARATOR_H
#define STRESSGENEARATOR_H
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <map>
#include <string>

#include "Logger.h"
#include "Utils.h"

typedef void sigfunc(int);
class StressGenerator {
 private:
  struct sockaddr_in servaddr_;        // epoll服务器地址
  static int exit_flag_;               // 退出标志
  int shutdown_flag_{0};               // 关闭标志
  int epollfd_;                        /* epoll描述符 */
  size_t cliCount_ = 0;                /* 要求的会话数 */
  size_t payloadSize_ = 0;             /* 每个报文的载荷大小 */
  char* payload_ = nullptr;            /* 初始化的数据包 */
  std::map<int, ClientInfo> clients_;  //客户端集合
  Logger* logger_;                     // 日志

  // 记录信息
  int connNum_{0};
  int unconnNum_{0};
  int recordFlag_{0};        // 是否发送报文
  uint64_t g_recvPackets{0}; /* 收到到报文数量 */
  uint64_t g_sendPackets{0}; /* 发送的报文数量 */
  double g_totalDelay{0};    /* 总延迟 */
  double g_averageDelay{0};  /* 报文平均延迟 */
 private:
  /**
   * @brief  生成报文
   *
   */
  void GeneratePacket();
  void AddDelay(struct timespec* timestamp);
  void AddClient(int sockfd, int state);
  int AddAllClients();
  int RemoveClient(int sockfd);
  uint16_t HandleHeaderWithTime(HeaderInfo* header, const size_t& fd);
  int EventsHandler(struct epoll_event* events, const int& nready);
  int EventsLoop(const char* ip, const char* port);
  void CloseAllClients();

 public:
  StressGenerator();
  ~StressGenerator();
  int StartPress(const char* ip, const char* port, int sessions,
                 int packetSize);
  sigfunc* Signal(int signo, sigfunc* func);
  static void SigIntHandler(int sig);
};
#endif /* STRESSGENEARATOR_H */