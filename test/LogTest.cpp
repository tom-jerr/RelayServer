#include "../include/Logger.h"
int main() {
  LOG_INFO("This is a test: %s\n", "infotest");
  delete Logger::GetInstance();
}