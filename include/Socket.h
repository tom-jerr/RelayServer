#ifndef SOCKET_H_
#define SOCKET_H_
#include "Logger.h"
#include "NetAddress.h"

class NSocket {
 public:
  NSocket() = default;
  explicit NSocket(int fd);
  ~NSocket();
  // NONCOPY
  NSocket(const NSocket &) = delete;
  NSocket &operator=(const NSocket &) = delete;

  int GetFd() const;

  void Connect(NetAddress &server_addr);
  /**
   * @brief 对Socket的文件描述符绑定对应地址
   *
   * @param server_addr
   * @param reusable 是否重用地址和端口
   */
  void Bind(NetAddress &server_addr, bool reusable);
  void Listen() const;
  int Accept(NetAddress &client_addr);

  void SetReusable();
  void SetNonBlocking();

 private:
  int fd_{-1};
  // Logger *logger_{nullptr};
};
#endif /* SOCKET_H_ */