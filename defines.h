#ifndef DEFINES_H
#define DEFINES_H

#define __RELEASE 0

#define FULL_LOGGING 0

#define HASH_BITS 256
#define HASH_BITS_STR "256"

#define BYTES_HASH (HASH_BITS / 8)
#define USED_HASH "sha" HASH_BITS_STR

#define HASH_HEX_BYTES (BYTES_HASH * 2)
#define HASH_HEX_BYTES_STR "64"

#define BUFFER_READ_SIZE (64 * 1024)

#define HASHING_BLOCK_SIZE 16
#define HASHING_BLOCK_SIZE_STR "16"

#define HASH_FILENAME_POSTFIX_NUMBERS 6

#define MAX_SINGLE_HASH_FILE_SIZE (1 << 31)

#define BLOCK_SIZE_BYTES 1

#define HASH_TABLE_NAME "hashes_" USED_HASH "_" HASHING_BLOCK_SIZE_STR

#define REAL_HASHED_BLOCK_SIZE_BYTES 2 // size of hashed fragment, placed in start of each fragment

#define SQL_REQUEST_LENGTH_LIMIT (64 * 1024) // used in help

#define READED_BLOCKS (BUFFER_READ_SIZE / HASHING_BLOCK_SIZE)

#define INSERT_MANY_HASHES_COUNT (BUFFER_READ_SIZE / HASHING_BLOCK_SIZE)

#define BIGSERIAL_MAX_NUMBERS 19    // 1 .. 9223372036854775807 + bigint
#define SERIAL_MAX_NUMBERS    10    // 1 .. 2147483647          + integer

#define SUBDIRECTORY_HASHES_PATH "/tmp/deduplicated_server/hashes"
#define SUBDIRECTORY_FILES_PATH "/tmp/deduplicated_server/files_" USED_HASH "_" HASHING_BLOCK_SIZE_STR
#define PREF_LAST_HASH_FILENAME "." USED_HASH "_" HASHING_BLOCK_SIZE_STR ".last"
#define wrap_ostringstream(X) (std::ostringstream() << X).str().data()

// 'HASH_HEX_BYTES',
#define SELECT_MANY_HASHES_LENGTH ((3 + HASH_HEX_BYTES) * SELECT_MANY_HASHES_COUNT - 1)

// ('HASH_HEX_BYTES',serial,bigserial,1),
#define INSERT_ROW_MAX_LENGTH (9 + HASH_HEX_BYTES + BIGSERIAL_MAX_NUMBERS + SERIAL_MAX_NUMBERS)

#define INSERT_MAX_MANY_HASHES_LENGTH (INSERT_ROW_MAX_LENGTH * INSERT_MANY_HASHES_COUNT - 1)

constexpr const char HASH_FILENAME_PREFIX[] = USED_HASH "_" HASHING_BLOCK_SIZE_STR "_";

#if (BUFFER_READ_SIZE % HASHING_BLOCK_SIZE)
#error BUFFERED_READ_SIZE % HASHING_BLOCK_SIZE must be 0;
#endif

#if (BUFFER_READ_SIZE % BYTES_HASH)
#error BUFFER_READ_SIZE % BYTES_HASH should be 0
#endif

#if (HASH_HEX_BYTES == 0)
#error "bad hash size"
#endif

#if (HASHING_BLOCK_SIZE >= (1 << (8 * BLOCK_SIZE_BYTES)))
#error "HASHING_BLOCK_SIZE overflow BLOCK_SIZE_BYTES"
#endif

#endif // DEFINES_H
