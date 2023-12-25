#include <filesystem>
#include <iostream>
#include <string.h>

#include "file.h"
#include "defines.h"

int main(int argc, char ** argv)
{
  if (argc != 3) {
    std::cerr << "bad command line args - need 2 files for compare\n";
    return -1;
  }
  if (!(std::filesystem::exists(std::filesystem::path(argv[1]))
     && std::filesystem::exists(std::filesystem::path(argv[2])))) {
    std::cerr << "files: \'" << argv[1] << "\', \'" << argv[2] << "\' not found, aborted...\n";
    return -2;
  }
  file_t first(argv[1], O_RDONLY);
  file_t second(argv[2], O_RDONLY);
  if (!(first.open() && second.open())) {
    std::cerr << "can't open files: " << strerror(errno) << std::endl;
    return -3;
  }
  std::string buffer1(BUFFER_READ_SIZE, 0);
  std::string buffer2(BUFFER_READ_SIZE, 0);
  size_t deltas = 0;
  ssize_t readed1 = 0;
  ssize_t readed2 = 0;
  ssize_t left1 = 0;
  ssize_t left2 = 0;
  size_t blocks;
  do {
    size_t block;
    size_t min = std::min(readed1 + left1, readed2 + left2);
    for (block = 0; block + HASHING_BLOCK_SIZE <= min; block += HASHING_BLOCK_SIZE) {
      if (memcmp(buffer1.data() + block, buffer2.data() + block, HASHING_BLOCK_SIZE) != 0) {
        deltas++;
      }
    }
    blocks += block / HASHING_BLOCK_SIZE;
    left1 = readed1 + left1 - block;
    left2 = readed2 + left2 - block;
    if (left1 > 0) memcpy(buffer1.data(), buffer1.data() + block, left1);
    if (left2 > 0) memcpy(buffer2.data(), buffer2.data() + block, left2);
    readed1 = first.read(buffer1.data() + left1, BUFFER_READ_SIZE - left1);
    readed2 = second.read(buffer2.data() + left2, BUFFER_READ_SIZE - left2);
  } while (readed1 > 0 && readed2 > 0);
  ssize_t min = std::min(readed1 + left1, readed2 + left2);
  if (min > 0) {
    blocks++;
    if (memcpy(buffer1.data(), buffer2.data(), min) != 0) {
      deltas++;
    }
    readed1 -= min;
    readed2 -= min;
  }
  if (readed1 + left1 > 0 || readed2 + left2 > 0) {
    if (readed1 + left1 > 0) {
      std::cout << "file \'" << argv[2] << "\' ended earlier.\n";
    } else {
      std::cout << "file \'" << argv[1] << "\' ended earlier.\n";
    }
  }
  std::cout << "deltas: " << deltas << ", all compared blocks: " << blocks
            << ", delta %: " << ((float) deltas) / blocks << std::endl;
  return 0;
}
