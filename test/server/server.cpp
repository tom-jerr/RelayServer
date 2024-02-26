#include "../include/Server.h"

int main() {
  RelayServer server;
  server.StartServer("127.0.0.1", 1234);
  return 0;
}