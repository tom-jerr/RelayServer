#include "RelayServer.h"

int main() {
  RelayServer server("127.0.0.1", 1234);
  server.Start();
  return 0;
}