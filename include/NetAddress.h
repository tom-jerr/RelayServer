#ifndef NETADDRESS_H_
#define NETADDRESS_H_

#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstring>
class NetAddress {
 public:
  NetAddress();
  NetAddress(const char *ip, uint16_t port);
  explicit NetAddress(const struct sockaddr_in &addr);
  ~NetAddress() = default;

  const char *GetIp() const;
  uint16_t GetPort() const;
  struct sockaddr *GetAddr();
  socklen_t *GetAddrLen();
  void SetAddr(const struct sockaddr_in &addr);

 private:
  struct sockaddr_in addr_;
  socklen_t addr_len_;
};

#endif /* NETADDRESS_H_ */