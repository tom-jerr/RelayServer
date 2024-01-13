#ifndef UTILS_H_
#define UTILS_H_
#include <arpa/inet.h>
#include <errno.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdio>
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
#define MAXEVENTS 64  // EPOLL最大事件数，use in epoll_create
#define MAXHANDLERS 64  // 最大的事件处理器数, use in epoll_event[MAXHANDLERS]
#define MAXLINES 1024                        // 一次读取的最大行数
#define BUFFERSZ 2048                        // Buffer初始大小
#define BACKLOG 128                          // listen队列的最大长度
#define GetDstId(x) (x % 2 ? x - 1 : x + 1)  // 获取目的客户端编号
#define HEADERSZ (sizeof(HeaderInfo))        // 头部大小
/*
  报文头信息
*/
struct HeaderInfo {
  size_t id;            // 报文编号
  size_t cliId;         // 客户端编号
  size_t len;           // 报文长度
  int connfd;           // 连接描述符
  bool head_recv;       // 报文头是否接收完毕
  bool body_recv;       // 报文体是否接收完毕
  size_t recv_idx = 0;  // 已经接收到的报文位置
  FILE* fp;  // 文件指针(对端为连接，持久化信息所在处)
};
/*
  报文信息
*/
struct MessageInfo {
  HeaderInfo header;      // 报文头
  char buffer[BUFFERSZ];  // 报文缓冲区
};
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
  向epoll中加入事件
*/
void AddFd(int epoll_fd, int fd, bool enable_out, bool enable_et);
/*
  修改epoll中的事件
*/
void ModFd(int epoll_fd, int fd, int ev);
/*
  从epoll中删除事件
*/
void DelFd(int epoll_fd, int fd);
#endif  // UTILS_H_