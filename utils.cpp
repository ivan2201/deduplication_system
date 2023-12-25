#include "utils.h"

#include "memory.h"

void to_my_hex(char * hex, const unsigned char * byte, size_t n) {
  for (size_t i = 0; i < n; i++) {
    hex[2 * i]     = 'A' + ((byte[i] >> 4) % 0x10);
    hex[2 * i + 1] = 'A' +  (byte[i]       % 0x10);
  }
}

size_t add_wrapped_with_delim_sql(char * buf, size_t left, const char * value, size_t len)
{
  if (left < len + 3) return 0;
  buf[0] = ',';
  buf[1] = '\'';
  memcpy(buf + 2, value, len);
  buf[len + 2] = '\'';
  return len + 3;
}

size_t add_wrapped_sql(char * buf, size_t left, const char * value, size_t len)
{
  if (left < len + 3 || len == 0) return 0;
  buf[0] = '\'';
  memcpy(buf + 1, value, len);
  buf[len + 1] = '\'';
  return len + 2;
}

void swap(size_t * a, size_t * b)
{
  size_t tmp = *a;
  *a = *b;
  *b = tmp;
}

void init_sort_indexes(size_t * sort_indexes, size_t len)
{
  for (size_t i = 0; i < len; i++) sort_indexes[i] = i;
}

// поразрядная сортировка, однако худший случай у неё, конечно, будет неприятный)))
// хотя, с отдельной сортировкой подмассива из 2ух элементов
// не так плохо: это случай с 10ью 3ойками и
// 1ой 4кой одинаковых hex с различными начальными символами -
// 11 раз будет достигнута максимальная глубина рекурсии в 64 (при сортировке 64 hex'ов)
void sort_my_hex(char * arr, size_t hex_len, size_t * sort_indexes, size_t hex_count,
              size_t power)
{
  if (power >= hex_len) return;
  if (hex_count < 2) return;
  if (hex_count == 2) {
    if (memcmp(arr + sort_indexes[0] * hex_len, arr + sort_indexes[1] * hex_len, hex_len) > 0)
      swap(sort_indexes, sort_indexes + 1);
    return;
  }
  size_t down = 0;
  size_t up = hex_count - 1;
  for (int i = 0; i < 8; i++) {
    size_t current = down;
    while (current < up) {
      if(arr[sort_indexes[current] * hex_len + power] == i) {
        if (down != current) swap(sort_indexes + down, sort_indexes + current);
        down++;
      } else if (arr[sort_indexes[current] * hex_len + power] == 'A' + (15 - i)) {
        swap(sort_indexes + up, sort_indexes + current);
        up--;
        continue;
      }
      current++;
    }
  }
  down = 0;
  for (char i = 0; i < 16; i++) {
    if (arr[sort_indexes[down] * hex_len + power] != 'A' + i) continue;
    up = down + 1;
    for (; up < hex_count; up++) {
      if (arr[sort_indexes[up] * hex_len + power] != 'A' + i) {
        sort_my_hex(arr, hex_len, sort_indexes + down, up - down, power + 1);
        down = up;
        break;
      }
    }
    if (up == hex_count) {
      sort_my_hex(arr, hex_len, sort_indexes + down, up - down, power + 1);
      break;
    }
  }
}

void strset(char * dest, char ch, size_t count)
{
  for (size_t i = 0; i < count; i++) {
    dest[i] = ch;
  }
}

size_t add_number(char * buf, size_t left, unsigned long long number)
{
  size_t power = 0;
  unsigned long long current = 10;
  const size_t max_digit = (sizeof(number) * 256) / 10;
  while (power < max_digit && current < number) {
    current *= 10;
    power++;
  }
  current /= 10;
  if (left < power) return 0;
  size_t writed = 0;
  while (power >= 0 && current > 0) {
    buf[writed++] = '0' + ((number / current) % 10);
    number = number % current;
    current /= 10;
    power--;
  }
  return writed;
}
