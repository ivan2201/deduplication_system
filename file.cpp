#include "file.h"

#include <errno.h>
#include <iostream>
#include <string.h>

file_t::file_t(std::string path, int open_mode)
  : path_(path)
  , mode_(open_mode)
  , fd_(-1)
{
}

file_t::file_t(file_t && other)
  : path_(std::exchange(other.path_, {}))
  , mode_(other.mode_)
  , fd_(std::exchange(other.fd_, -1))
{
}

file_t::~file_t()
{
  close();
}

file_t& file_t::operator =(file_t&& other)
{
  ::close(fd_);
  fd_ = std::exchange(fd_, other.fd_);
  return *this;
}

bool file_t::open()
{
  fd_ = handle_eintr(::open, path_.c_str(), mode_);
  if (fd_ < 0) return false;
  return true;
}

void file_t::close()
{
  if (fd_ >= 0)
    ::close(fd_);
}

file_t::operator bool()
{
  return fd_ >= 0;
}

ssize_t file_t::read(char* buff, off_t count)
{
  if (fd_ < 0) return -1;
  return check_result(::read(fd_, buff, count));
}

ssize_t file_t::read(off_t pos, char* buff, off_t count)
{
  if (fd_ < 0) return -1;
  lseek(fd_, pos, SEEK_SET);
  return read(buff, count);
}

off_t file_t::to_begin()
{
  if (fd_ < 0) return -1;
  return lseek(fd_, 0, SEEK_SET);
}

off_t file_t::to_end()
{
  if (fd_ < 0) return -1;
  return lseek(fd_, 0, SEEK_END);
}

off_t file_t::truncate(off_t lenght)
{
  if (fd_ < 0 || lenght < 0) return -1;
  if (check_result(ftruncate(fd_, lenght)) != 0) {
    return -1;
  }
  return lseek(fd_, lenght, SEEK_SET);
}

ssize_t file_t::write(const char* buff, off_t count)
{
  if (fd_ < 0) return -1;
  return check_result(::write(fd_, buff, count));
}

ssize_t file_t::write(off_t pos, const char* buff, off_t count)
{
  if (fd_ < 0) return -1;
  lseek(fd_, pos, SEEK_SET);
  return write(pos, buff, count);
}

off_t file_t::position()
{
  if (fd_ < 0) return -1;
  return check_result(lseek(fd_, 0, SEEK_CUR));
}

template<typename result_type>
result_type file_t::check_result(result_type result)
{
  if(result < 0) {
    std::cerr << "error occurred while file operation" << strerror(errno) << std::endl;
  }
  return result;
}
