#ifndef FILE_H
#define FILE_H
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <utility>

namespace  {

template<typename Operation, typename... Args>
int handle_eintr(Operation operation, Args... args)
{
  int result;
  while ((result = operation(args...)) == -1 && errno == EINTR);
  return result;
}

} // anonimous namespace

class file_t
{
public:
  file_t(std::string path, int open_mode);

  file_t(const file_t& other) = delete;

  file_t(file_t && other);

  ~file_t();

  file_t& operator =(file_t&& other);

  bool open();

  void close();

  operator bool();

  ssize_t read(char* buff, off_t count);

  ssize_t read(off_t pos, char* buff, off_t count);

  ssize_t write(const char* buff, off_t count);

  ssize_t write(off_t pos, const char* buff, off_t count);

  off_t truncate(off_t lenght = 0);

  off_t to_begin();

  off_t to_end();

  off_t position();

  std::string path() { return path_; }

private:
  std::string path_;

  template<typename result_type>
  result_type check_result(result_type result);

  int mode_;
  int fd_;
};

#endif // FILE_H
