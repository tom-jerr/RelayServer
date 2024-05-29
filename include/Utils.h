#ifndef UTILS_H_
#define UTILS_H_
#include <arpa/inet.h>
#include <bits/types/struct_timespec.h>
#include <errno.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdio>
#include <utility>
#define ASSERT(x)                                \
  do {                                           \
    if (!(x)) {                                  \
      printf("Assertion failed: %s \t", #x);     \
      printf("At %s: %d\n", __FILE__, __LINE__); \
      strerror(errno);                           \
      exit(1);                                   \
    }                                            \
  } while (0)

/*
  Params Settings
*/
#define MAXEVENTS 10010  // EPOLL最大事件数，use in epoll_create
#define MAXHANDLERS \
  10010  // 最大的事件处理器数, use in epoll_event[MAXHANDLERS]
#define MAXCONN MAXHANDLERS                  // 最大连接数
#define MAXCHARS 30001                       // 最大字符数 + 1
#define BUFFERSZ 100000                      // Buffer初始大小
#define BACKLOG 10010                        // listen队列的最大长度
#define GetDstId(x) (x % 2 ? x - 1 : x + 1)  // 获取目的客户端编号
#define HEADERSZ (sizeof(HeaderInfo))        // 头部大小
/*
  通信收包状态
*/
#define _NOT_CONN -1    // 还未连接
#define _PKG_HD_INIT 0  //初始状态，准备接收数据包头
#define _PKG_BD_INIT 2  //包头刚好收完，准备接收包体

/*
  报文头信息：仅仅该信息需要网络传输
*/
#pragma pack(1)
struct HeaderInfo {
  uint32_t cliId;  // 客户端编号
  uint32_t len;    // 报文长度(不包括头部)
  uint64_t sec;    /* UTC：秒数 */
  uint64_t nsec;   /* UTC：纳秒数 */
};
#pragma pack()
/*
  报文信息：服务器内部维护信息
  recvlen：最终长度为头部长度+报文体长度
  unrecvlen：不停在头部长度和报文体长度间切换
*/
struct MessageInfo {
  HeaderInfo header;         // 报文头
  uint32_t connfd;           // 连接描述符
  uint32_t cliId;            // 客户端编号
  int shutwr = 0;            // 是否关闭写端
  bool head_recv = false;    // 报文头是否接收完毕
  bool body_recv = false;    // 报文体是否接收完毕
  int recvlen = 0;           // 已经接收的报文长度
  int unrecvlen = HEADERSZ;  // 未接收的报文长度
  char buffer[BUFFERSZ];     // 报文缓冲区
};
/*
  压力发生器用户缓冲区
*/
struct ClientBuffer {
  HeaderInfo recv;           // 用户接收报文的头部信息
  HeaderInfo send;           // 用户发送报文的头部信息
  bool head_recv = false;    // 报文头是否接收完毕
  int recvlen = 0;           // 已经接收的报文长度
  int unrecvlen = HEADERSZ;  // 未接收的报文长度
  char buffer[BUFFERSZ];     // 报文缓冲区
  char* sendpackets;         // 发送的报文
  int sendlen = 0;           // 发送的报文长度
};
/*
  客户端信息
*/
struct ClientInfo {
  int connfd;
  int state{-1};  // -1: 未连接 0: 已连接 1: 关闭写端
  ClientBuffer* buffer{nullptr};
};
/* 将64字节变量从网络字节序变为主机字节序 */
uint64_t ntoh64(uint64_t net64);

/* 将64字节变量从主机字节序变为网络字节序 */
uint64_t hton64(uint64_t host64);
/*
  解析头部信息
*/
std::pair<uint32_t, uint32_t> ParseHeader(HeaderInfo* header);
/*
  获取一个带时间的头部
*/
struct timespec GetHeader(uint16_t length, uint32_t id, HeaderInfo* header);
/*
  地址转换函数
*/
int InetPton(int af, const char* src, void* dst);
/*
  设置非阻塞，并返回旧的File flags
*/
int SetNonBlocking(int fd);
/*
  创建Socket
*/
int Socket(int domain, int type, int protocol);
/*
  绑定socket
*/
int Bind(int fd, const struct sockaddr* servaddr, socklen_t len);
/*
  监听Socket
*/
int Listen(int fd, int n);
/*
  连接Socket
*/
int Connect(int fd, const struct sockaddr* servaddr, socklen_t len);
/*
  向fd中写入数据
*/
int Write(int fd, const void* buffer, size_t len);
/*
  从fd中读取数据
*/
int Read(int fd, void* buffer, size_t len);
/*
  关闭fd
*/
int Close(int fd);
/*
  向epoll中加入事件
*/
void AddFd(int epoll_fd, int fd, bool enable_out, bool enable_et);
void AddOutFd(int epoll_fd, int fd);
/*
  修改epoll中的事件
*/
void ModFd(int epoll_fd, int fd, int ev);
/*
  从epoll中删除事件
*/
void DelFd(int epoll_fd, int fd);
#endif  // UTILS_H_