#ifndef COMMON_H_
#define COMMON_H_
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
/*
  Params Settings
*/
#define MAXEVENTS 10010  // EPOLL最大事件数，use in epoll_create
#define MAXHANDLERS \
  10010  // 最大的事件处理器数, use in epoll_event[MAXHANDLERS]
#define MAXCHARS 30001                       // 最大字符数 + 1
#define BUFFERSZ 140000                      // Buffer初始大小
#define BACKLOG 10010                        // listen队列的最大长度
#define GetDstId(x) (x % 2 ? x - 1 : x + 1)  // 获取目的客户端编号
#define HEADERSZ (sizeof(HeaderInfo))        // 头部大小

/*
  通信收包状态
*/
#define _PKG_HD_INIT 0     //初始状态，准备接收数据包头
#define _PKG_HD_RECVING 1  //接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT 2     //包头刚好收完，准备接收包体
#define _PKG_BD_RECVING \
  3  //接收包体中，包体不完整，继续接收中，处理后直接回到_PKG_HD_INIT状态

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

#endif /* COMMON_H_ */