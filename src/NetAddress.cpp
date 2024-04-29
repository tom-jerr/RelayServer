#include "NetAddress.h"

NetAddress::NetAddress() {
  memset(&addr_, 0, sizeof(addr_));
  addr_len_ = sizeof(addr_);
}
NetAddress::NetAddress(const char *ip, uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr_.sin_addr);
}

NetAddress::NetAddress(const struct sockaddr_in &addr) : addr_(addr) {}

const char *NetAddress::GetIp() const { return inet_ntoa(addr_.sin_addr); }

uint16_t NetAddress::GetPort() const { return ntohs(addr_.sin_port); }

struct sockaddr *NetAddress::GetAddr() {
  return reinterpret_cast<struct sockaddr *>(&addr_);
}

socklen_t *NetAddress::GetAddrLen() { return &addr_len_; }

void NetAddress::SetAddr(const struct sockaddr_in &addr) { this->addr_ = addr; }