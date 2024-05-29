#ifndef CONNECTION_H_
#define CONNECTION_H_
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>

#include "Socket.h"
#include "Utils.h"

class Connection {
 public:
  enum { RECVBUF, SENDBUF };
  explicit Connection(int fd) : m_conn_(new NSocket(fd)) {}
  ~Connection() {
    if (m_conn_ != nullptr) {
      delete m_conn_;
    }
    // if (m_precvbuf_ != nullptr) {
    //   delete m_precvbuf_;
    // }
    // if (m_psendbuf_ != nullptr) {
    //   delete m_psendbuf_;
    // }
  }
  // NONCOPY
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  int GetFd() { return m_conn_->GetFd(); }
  NSocket* GetSocket() { return m_conn_; }

  int GetCliId() { return m_cliId_; }
  void SetCliId(int id) { m_cliId_ = id; }

  int GetdstId() { return m_dstId_; }
  void SetdstId(int id) { m_dstId_ = id; }

  int GetState() { return m_state_; }
  void SetState(int state) { m_state_ = state; }

  bool GetShutWR() { return m_shurwr_; }
  void SetShutWR(bool flag) { m_shurwr_ = flag; }

  char* GetHeadBuf() { return m_headbuf_; }
  char* GetRecvBuf() { return m_precvbuf_; }
  // char* GetSendBuf() { return m_psendbuf_; }

  // void AllocBuf(int flags, size_t size) {
  //   if (flags) {
  //     if (m_psendbuf_ == nullptr) {
  //       m_psendbuf_ = (char*)malloc(size);
  //     }
  //   } else {
  //     if (m_precvbuf_ == nullptr) {
  //       m_precvbuf_ = (char*)malloc(size);
  //     }
  //   }
  // }

  int GetPacketLen() { return m_packetlen_; }
  void SetPacketLen(int len) { m_packetlen_ = len; }

  int GetRecvLen() { return m_irecvlen_; }
  void AddRecvLen(int len) {
    m_irecvlen_ += len;
    // m_iunrecvlen_ -= len;
  }
  void SubRecvLen(int len) { m_irecvlen_ -= len; }

  int GetUnRecvLen() { return m_iunrecvlen_; }
  void SetUnRecvLen(int len) { m_iunrecvlen_ = len; }
  // void AddUnRecvLen(int len) { m_iunrecvlen_ += len; }
  // void SubUnRecvLen(int len) { m_iunrecvlen_ -= len; }
  // int GetSendLen() { return m_isendlen_; }
  // void AddSendlen(int len) { m_isendlen_ += len; }
  // void ResetSendlen() { m_isendlen_ = 0; }
  // 类函数需要将this指针传入
  void SetReadcb(const std::function<void(Connection*)>& readcb) {
    m_read_cb_ = [readcb, this] { return readcb(this); };
  }
  void SetWritecb(const std::function<void(Connection*)>& writecb) {
    m_write_cb_ = [writecb, this] { return writecb(this); };
    ;
  }

  std::function<void()> GetReadcb() { return m_read_cb_; }
  std::function<void()> GetWritecb() { return m_write_cb_; }

 private:
  NSocket* m_conn_;         // 连接Socket
  int m_cliId_{-1};         // 客户端编号
  int m_dstId_{-1};         // 对端会话
  int m_state_{_NOT_CONN};  // 连接状态
  /*
    收包
  */
  int m_packetlen_{0};        // 需要接收的包体长度
  char m_headbuf_[HEADERSZ];  // 数据包头信息
  int m_irecvlen_{0};         // 接收长度
  int m_iunrecvlen_{HEADERSZ};  // 还未接收的长度；在HEADERSZ与PacketLen之间变换
  char m_precvbuf_[BUFFERSZ];  // 接收缓冲区

  // /*
  //   发包
  // */
  // std::atomic<int> iSendCount{0};  // 发送计数
  // // char* m_psendmem_;               // 发送首地址
  // int m_isendlen_{0};          // 发送长度
  // char m_psendbuf_[BUFFERSZ];  // 发送缓冲区
  /*
    写端是否关闭
  */
  bool m_shurwr_{false};
  /*
    回调函数
  */
  std::function<void()> m_read_cb_;
  std::function<void()> m_write_cb_;
};
#endif /* CONNECTION_H_ */