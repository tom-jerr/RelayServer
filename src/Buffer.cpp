#include "../include/Buffer.h"

#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

Buffer::Buffer() : read_index_(kCheapPrepend), write_index_(kCheapPrepend) {
  assert(ReadableBytes() == 0);
  assert(PrependableBytes() == kCheapPrepend);
  assert(WritableBytes() == kInitialSize);
}

std::size_t Buffer::ReadableBytes() const { return write_index_ - read_index_; }

std::size_t Buffer::WritableBytes() const { return size() - write_index_; }

std::size_t Buffer::PrependableBytes() const { return read_index_; }

int Buffer::MakeSpace(std::size_t len) {
  if (len <= WritableBytes()) return 0;
  if (len <= WritableBytes() + ReadableBytes()) {
    std::size_t usable_space = size() - ReadableBytes();
    memcpy(begin() + kCheapPrepend, begin() + read_index_, usable_space);
    read_index_ = kCheapPrepend;
    write_index_ = read_index_ + usable_space;
    return 0;
  }
  return 1;
}

void Buffer::Append(const char* data, std::size_t len) {
  if (len < WritableBytes()) {
    std::copy(data, data + len, begin() + write_index_);
    write_index_ += len;
  } else {
    // 此时没有足够的空间一次性写入
    // MakeSpace后，read_index_和write_index_重置
    int ret = MakeSpace(len);
    if (ret == 0) {
      std::copy(data, data + len, begin() + write_index_);
      write_index_ += len;
    } else {
      std::cout << "Need to wait enough space\n";
    }
  }
}

std::size_t Buffer::ReadFromFd(int fd, std::size_t len) {
  const std::size_t writable = WritableBytes();
  // 一次性读取len个字节
  const std::size_t n = read(fd, begin() + read_index_, len);
  if (n < 0) {
    std::cout << "readv error\n";
    return -1;
  }
  if (n <= writable) {
    write_index_ += n;
    return n;
  }
  // 一次性读取的字节数大于Buffer中可读的字节数
  std::cout << "readv read " << n << " bytes" << "; need to read "
            << len - writable << " bytes more\n";
  return -1;
}

std::size_t Buffer::WriteToFd(int fd, std::size_t len) {
  if (len > ReadableBytes()) {
    std::cout << "Need to waite read from fd\n";
    return -1;
  }
  // 从Buffer中读取len个字节到fd中
  std::size_t n = write(fd, begin() + read_index_, len);
  if (n < 0) {
    std::cout << "write error\n";
    return -1;
  }
  read_index_ += n;
  return n;
}