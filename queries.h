#ifndef QUERIES_H
#define QUERIES_H

#include "defines.h"

constexpr const char * CREATE_HASH_TABLE = "CREATE TABLE if not exists " HASH_TABLE_NAME " ("
                                           "hash  char(" HASH_HEX_BYTES_STR ") primary key,"
                                           "file  integer,"
                                           "pos   bigint,"
                                           "count integer"
                                           ");";

constexpr const char * CREATE_FILE_TABLE = "CREATE TABLE if not exists used_files ("
                                           "id   serial primary key,"
                                           "path varchar(256)"
                                           ");";

constexpr const char * SQL_QUARY_SCOPE_END = ");";

constexpr const char SELECT_FILE_POS_FROM_HASHES_MANY[] =
  "select hash,file,pos from " HASH_TABLE_NAME " where hash in (";

constexpr const char INSERT_MANY_CACHES[] =
  "insert into " HASH_TABLE_NAME " values ";

constexpr const char INSERT_HASH_COUNT_END[] = ",1)";

constexpr const char SELECT_EXISTS_HASHES_MANY[] =
  "select hash from " HASH_TABLE_NAME " where hash in (";

constexpr const char * SELECT_FILES_FROM_DB = "SELECT id,path from used_files;";

constexpr const char EXISTS_HASH[] =
  "select 1 from " HASH_TABLE_NAME " where hash = ";

constexpr const char SELECT_FILE_POS_FROM_HASHES[] =
  "select file,pos from " HASH_TABLE_NAME " where hash = ";

constexpr const char SELECT_FILE_ID[] =
  "select id from used_files where path = ";

constexpr const char INSERT_HASH_FILE[] =
  "insert into used_files (path) values (";

#endif // QUERIES_H
