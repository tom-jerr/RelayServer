#ifndef MEMORY_H_
#define MEMORY_H_

class CMemory {
 private:
  CMemory() = default;
  static CMemory *m_instance;

 public:
  ~CMemory();

 public:
  static CMemory *GetInstance()  //单例
  {
    if (m_instance == nullptr) {
      m_instance = new CMemory();
    }

    return m_instance;
  }

 public:
  void *AllocMemory(int memCount, bool ifmemset);
  void FreeMemory(void *point);
};

#endif /* MEMORY_H_*/