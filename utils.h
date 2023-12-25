#ifndef UTILS_H
#define UTILS_H

#include <cstddef>

void to_my_hex(char * hex, const unsigned char * byte, size_t n);

// return writed bytes
size_t add_wrapped_with_delim_sql(char * buf, size_t left, const char * value, size_t len);

size_t add_wrapped_sql(char * buf, size_t left, const char * value, size_t len);

size_t add_number(char * buf, size_t left, unsigned long long number);

void init_sort_indexes(size_t * sort_indexes, size_t len);

void sort_my_hex(char * arr, size_t hex_len, size_t * sort_indexes, size_t hex_count,
              size_t power = 0);

void strset(char * dest, char ch, size_t count);

#endif // UTILS_H
