#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "../include/Buffer.h"
#include "../include/Utils.h"
void BufferInfo(Buffer &buffer) {
  std::cout << "Buffer's readable:\t" << buffer.ReadableBytes() << std::endl;
  std::cout << "Buffer's writable:\t" << buffer.WritableBytes() << std::endl;
  std::cout << "Buffer's prependable:\t" << buffer.PrependableBytes()
            << std::endl;
}
int main() {
  Buffer buffer;
  int fd = open("test.txt", O_RDWR | O_CREAT, 0666);
  std::cout << "--------------ReadFromFd--------------\n";
  int ret = buffer.ReadFromFd(fd, 10);
  ASSERT(ret == 10);
  // std::cout << "Read from fd ret:\t" << ret << std::endl;
  BufferInfo(buffer);
  std::cout << "--------------WriteToFd--------------\n";
  int dst = open("dst.txt", O_RDWR | O_CREAT, 0666);
  ret = buffer.WriteToFd(dst, 10);
  ASSERT(ret == 10);
  // std::cout << "Write to fd ret:\t" << ret << std::endl;
  BufferInfo(buffer);
}