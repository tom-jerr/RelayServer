#ifndef STRESSGENEARATOR_H
#define STRESSGENEARATOR_H
#include <arpa/inet.h>
#include <bits/types/struct_timespec.h>
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
  int epollfd_;                        // epoll描述符
  int cliCount_;                       // 客户端数量
  size_t payloadSize_ = 0;             // 发送报文体大小
  char* packet_ = nullptr;             // 发送的报文体
  std::map<int, ClientInfo> clients_;  //客户端集合
  Logger* logger_;                     //日志

  // 记录信息
  int connNum_{0};
  int unconnNum_{0};
  int recordFlag_{0};           // 是否发送报文
  uint64_t g_recvPackets{0};    // 收到到报文数量
  uint64_t g_sendPackets{0};    // 发送的报文数量
  double g_totalDelay{0};       // 总延迟
  double g_averageDelay{0};     // 报文平均延迟
  struct timespec g_startTime;  // 压力测试开始时间
  struct timespec g_endTime;    // SIGINT接收时间
 private:
  /**
   * @brief  生成报文
   *
   */
  void GeneratePacket();
  /**
   * @brief 计算单个报文延迟，并更新总延迟
   *
   * @param timestamp
   */
  void CalcDelay(struct timespec* timestamp);
  /**
   * @brief 添加客户端
   *
   * @param sockfd
   * @param state
   */
  void AddClient(int sockfd, int state);
  /**
   * @brief 每次添加两个客户端连接；直到达到指定的客户端数量
   *
   * @return int
   */
  int AddDoubleClients();
  /**
   * @brief 移除sockfd对应的客户端
   *
   * @param sockfd
   * @return int
   */
  int RemoveClient(int sockfd);
  /**
   * @brief 解析报文头部，并更新报文延迟；返回报文长度
   *
   * @param header
   * @param fd
   * @return uint16_t
   */
  uint16_t HandleHeaderWithTime(HeaderInfo* header, const size_t& fd);
  /**
   * @brief 处理可连接事件、可读事件、可写事件
   *
   * @param events
   * @param nready
   * @return int
   */
  int EventsHandler(struct epoll_event* events, const int& nready);
  /**
   * @brief
   * 事件循环，重复调用AddDoubleClients()、EventsHandler()；直到退出标志为真
   *
   * @param ip
   * @param port
   * @return int
   */
  int EventsLoop(const char* ip, const char* port);
  /**
   * @brief 关闭所有客户端
   *
   */
  void CloseAllClients();

 public:
  StressGenerator();
  ~StressGenerator();
  /**
   * @brief 开启压力发生器
   *
   * @param ip
   * @param port
   * @param sessions
   * @param packetSize
   * @return int
   */
  void StartStress(const char* ip, const char* port, int sessions,
                   int packetSize);
  sigfunc* Signal(int signo, sigfunc* func);
  static void SigIntHandler(int sig);
};
#endif /* STRESSGENEARATOR_H */