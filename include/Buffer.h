#ifndef BUFFER_H_
#define BUFFER_H_

#include <cstddef>

#include "Utils.h"
static const std::size_t kCheapPrepend = 8;  // 初始预留的prependable空间大小
static const std::size_t kInitialSize = BUFFERSZ;  // Buffer初始大小

class Buffer {
 private:
  char data_[kCheapPrepend + kInitialSize];
  std::size_t read_index_;
  std::size_t write_index_;

 public:
  Buffer();
  ~Buffer() = default;

  /*
    可读区域：[read_index_, write_index_)
    可写区域：[write_index_, data_.size())
    prependable区域：[0, read_index_)
  */
  std::size_t ReadableBytes() const;
  std::size_t WritableBytes() const;
  std::size_t PrependableBytes() const;
  /*
    从fd中读取数据, 并将数据写入Buffer中
    向fd中写入数据, 从Buffer中读取数据
  */
  std::size_t ReadFromFd(int fd, std::size_t len);
  std::size_t WriteToFd(int fd, std::size_t len);

 private:
  /*
    向Buffer中添加数据
  */
  void Append(const char* data, std::size_t len);
  char* begin() { return data_; }
  const char* begin() const { return data_; }
  std::size_t size() const { return sizeof(data_); }
  /*
    确保Buffer中有足够的空间
  */
  int MakeSpace(std::size_t len);
};
#endif  // BUFFER_H_