#ifndef LOGGER_H_
#define LOGGER_H_
#include <cstdio>
#include <string>
class Logger {
 private:
  FILE *log_fp_;

 public:
  enum LogLevel { INFO, WARNING, ERROR };
  Logger(const char *filename);
  ~Logger();
  void Log(enum Logger::LogLevel level, const char *format, ...);
  std::string GetTime();
};
#endif  // LOGGER_H_