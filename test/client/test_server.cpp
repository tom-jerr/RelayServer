#include <arpa/inet.h>
#include <errno.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../../include/Utils.h"

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
  Bind(sockfd, (struct sockaddr *)&client_adddr, sizeof(client_adddr));
  Listen(sockfd, 128);

  // logger.Log(Logger::INFO, "Connected to server %s:%s\n", ipaddr.c_str(),
  //            port.c_str());
  // logger.Log(Logger::INFO, "Connected sockfd %d\n", sockfd);
  char buf[3];
  while (true) {
    socklen_t addrlen;
    auto connfd = accept(sockfd, (struct sockaddr *)&client_adddr, &addrlen);
    int ret = read(connfd, buf, sizeof(buf));
    if (ret > 0) {
      buf[ret] = '\0';
    }
    write(connfd, buf, sizeof(buf));
  }

  Close(sockfd);
}