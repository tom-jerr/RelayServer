#include "../include/StressGenerator.h"

int main(int argc, char** argv) {
  if (argc != 5) {
    printf(
        "usage: PressureGenerator <IP_Address> <Port> <Seession_Count> "
        "<Packet_Size>\n");
    return 0;
  }
  int sessionCount = atoi(argv[3]);
  int packetSize = atoi(argv[4]);
  StressGenerator generator;
  generator.StartPress(argv[1], argv[2], sessionCount, packetSize);
  return 0;
}