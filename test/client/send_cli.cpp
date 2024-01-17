#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "../include/Logger.h"
#include "../include/Utils.h"
/*
  客户端Log
*/
Logger logger = Logger("send_client.log");
void SendMSG(FILE *fp, int sockfd) {
  char sendline[MAXCHARS + HEADERSZ + 1];
  HeaderInfo header;
  // 将头部预留出大小
  while (fgets(sendline + HEADERSZ, MAXCHARS + 1, fp) != NULL) {
    int sendlen = strnlen(sendline + sizeof(HeaderInfo), MAXCHARS + 1);
    printf("Length of message: %d\n", sendlen);
    sendline[sendlen + HEADERSZ - 1] = '\0';
    // 通过网络传输，需要改变字节序
    printf("Transfer length: %ld\n", sendlen + HEADERSZ);
    header.len = htons(sendlen);
    memcpy(sendline, &header, HEADERSZ);

    logger.Log(Logger::INFO, "Send message: %s\n",
               sendline + sizeof(HeaderInfo));

    Write(sockfd, sendline, sendlen + sizeof(HeaderInfo));
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
  printf("Connected to server %s:%s\n", ipaddr.c_str(), port.c_str());
  SendMSG(stdin, sockfd);
  Close(sockfd);
}