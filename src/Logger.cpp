#include "../include/Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
Logger::Logger(const char* filename) { log_fp_ = fopen(filename, "w"); }

Logger::~Logger() { fclose(log_fp_); }

std::string Logger::GetTime() {
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
  return buf;
}

void Logger::Log(enum Logger::LogLevel level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  switch (level) {
    case INFO:
      fprintf(log_fp_, "[INFO]: %s\t", GetTime().c_str());
      break;
    case WARNING:
      fprintf(log_fp_, "[WARNING]: %s\t", GetTime().c_str());
      break;
    case ERROR:
      fprintf(log_fp_, "[ERROR]: %s\t", GetTime().c_str());
      break;
    default:
      fprintf(log_fp_, "[INFO]: %s\t", GetTime().c_str());
      break;
  }
  vfprintf(log_fp_, format, args);
  fprintf(log_fp_, "\n");
  fflush(log_fp_);
  va_end(args);
}