#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../include/Logger.h"
#include "../include/Utils.h"
/*
  客户端Log
*/
Logger logger = Logger("send_recv_client.log");

ssize_t Readn(int fd, void *vptr, size_t n) {
  size_t nleft = 0;
  ssize_t nread = 0;
  char *ptr;

  ptr = (char *)vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0; /* and call read() again */
      else
        return (-1);
    } else if (nread == 0)
      break; /* EOF */

    nleft -= nread;
    ptr += nread;
  }
  if (n - nleft < 0) {
    logger.Log(Logger::ERROR, "Client readn error\n");
    exit(1);
  }

  return (n - nleft); /* return >= 0 */
}

void RecvMSG(FILE *fp, int sockfd) {
  char recvline[MAXCHARS + 1];
  int recvlen;
  HeaderInfo header;  // 收到的数据包的Header
  int ret;
  // while (1) {
  ret = Readn(sockfd, &header, sizeof(header));
  if (ret == 0) {
    return;
  }
  if (ret != sizeof(header)) {  // 读取header
    fprintf(stderr, "echo_rpt: server terminated prematurely\n");
    return;
  }

  recvlen = ntohs(header.len);
  ret = Readn(sockfd, recvline, recvlen);
  if (ret != recvlen)  // 读取报文内容
  {
    fprintf(stderr, "echo_rpt: server terminated prematurely\n");
    return;
  }

  // 打印recvline数组
  std::cout << recvline << "\n";
  logger.Log(Logger::INFO, recvline);
  // }
}

void SendMSG(FILE *fp, int sockfd) {
  char sendline[MAXCHARS + HEADERSZ];
  HeaderInfo header;
  // 将头部预留出大小
  while (true) {
    if (fgets(sendline + HEADERSZ, MAXCHARS + 1, fp) != NULL) {
      int sendlen = strnlen(sendline + HEADERSZ, MAXCHARS + 1);
      // printf("Length of message: %d\n", sendlen);
      sendline[sendlen + HEADERSZ - 1] = '\0';
      // 通过网络传输，需要改变字节序
      // printf("Transfer length: %ld\n", sendlen + HEADERSZ);
      header.len = htons(sendlen);
      header.cliId = htonl(1);
      // printf("net byte order: %d\n", header.len);
      memcpy(sendline, &header, HEADERSZ);

      logger.Log(Logger::INFO, "Send message: %s\n",
                 sendline + sizeof(HeaderInfo));

      Write(sockfd, sendline, sendlen + HEADERSZ);
    }
    RecvMSG(fp, sockfd);
  }
}

int main(int argc, char *argv[]) {
  /*
   客户端连接服务器
  */
  int sockfd;
  struct sockaddr_in client_adddr;
  std::string ipaddr;
  std::string port;
  if (argc != 3) {
    ipaddr = "127.0.0.1";
    port = "1234";
  } else {
    ipaddr = argv[1];
    port = argv[2];
  }
  sockfd = Socket(AF_INET, SOCK_STREAM, 0);
  bzero(&client_adddr, sizeof(client_adddr));

  client_adddr.sin_family = AF_INET;
  client_adddr.sin_port = htons(atoi(port.c_str()));
  InetPton(AF_INET, ipaddr.c_str(), &client_adddr.sin_addr);
  Connect(sockfd, (struct sockaddr *)&client_adddr, sizeof(client_adddr));
  logger.Log(Logger::INFO, "Connected to server %s:%s\n", ipaddr.c_str(),
             port.c_str());
  logger.Log(Logger::INFO, "Connected sockfd %d\n", sockfd);
  printf("Connected to server %s:%s sockfd %d\n", ipaddr.c_str(), port.c_str(),
         sockfd);
  SendMSG(stdin, sockfd);

  Close(sockfd);
}