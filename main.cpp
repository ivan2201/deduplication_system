#include <assert.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sys/resource.h>

#include <postgresql/libpq-fe.h>

#include <openssl/sha.h>

#include "defines.h"
#include "deque.h"
#include "file.h"
#include "queries.h"
#include "utils.h"

#if (!__RELEASE)
#define soft_assert(expr)                                             \
  if (!(expr)) {                                                      \
    soft_close_all();                                                 \
    std::cerr << "assert failed: " << #expr << ", file: " << __FILE__ \
              << ", line: " << __LINE__ << std::endl;                 \
    abort();                                                          \
  }
#else
#define soft_assert(X)                                                \
  if (!(X)) {                                                         \
    exit_error(wrap_ostringstream("assert failed: " << #X ), 126);    \
  }
#endif

enum file_operation_t {
  NONE   = 0,
  READ   = 1,
  WRITE  = 2
};

std::filesystem::path files_dir;
std::filesystem::path hashes_dir;

size_t max_fd = 10;
size_t opened_fd = 0;

PGconn* dbconn = nullptr;

deque_t<file_t> files;

// key - id
std::map<std::string, deque_t<file_t>::iterator> hashes_files;

deque_t<file_t>::iterator output_hash_file;
deque_t<file_t>::iterator requested_file;

std::string output_hash_id;

void soft_close_all() {
  files.remove_all();
  if (dbconn) PQfinish(dbconn);
}

void exec_conn(PGresult* res, ExecStatusType expected, const char * error_prefix) {
  if (PQresultStatus(res) != expected) {
    std::cerr << error_prefix << ": " << PQresultStatus(res) << ", "
              << PQerrorMessage(dbconn) << std::endl;
    PQclear(res);
    soft_close_all();
    exit(1);
  }
}

void exit_error(const char * error_msg, int exit_code) {
  std::cerr << error_msg << std::endl;
  soft_close_all();
  exit(exit_code);
}

static void
noNoticeProcessor(void *arg, const char *message)
{
}

//
// if future, when fd limit will be ocurred then
// we can use binary_tree for searching useless opened fd and close it
// and open new. Now my system will close
// alse we can close oldest used fd
deque_t<file_t>::iterator openfile(std::string path, int open_mode) {
  file_t file(path, open_mode);
  if (opened_fd < max_fd) {
    soft_assert(file.open());
  } else {
    exit_error("error: limit files occurred", 10);
  }
  return files.add(std::move(file));
}

std::string select_or_insert_file(std::string path) {
  PGresult* res;
  static std::string select_id(sizeof(SELECT_FILE_ID) + 258, 0);
  static std::string insert_file(sizeof(INSERT_HASH_FILE) + 259, 0);
  static bool inited = false;
  if (!inited) {
    memcpy(select_id.data(), SELECT_FILE_ID, sizeof(SELECT_FILE_ID) - 1);
    memcpy(insert_file.data(), INSERT_HASH_FILE, sizeof(INSERT_HASH_FILE) - 1);
    inited = true;
  }
  const size_t end = sizeof(SELECT_FILE_ID) - 1 +
                     add_wrapped_sql(select_id.data() + sizeof(SELECT_FILE_ID) - 1, 258, path.data(), path.size());
  select_id.data()[end] = ';';
  res = PQexec(dbconn, select_id.data());
  exec_conn(res, PGRES_TUPLES_OK, "error: cann't select file");
  std::string result;
  if (PQntuples(res) > 0) {
    result = PQgetvalue(res, 0, 0);
  } else {
    PQclear(res);
    const size_t end = sizeof(INSERT_HASH_FILE) - 1 +
                       add_wrapped_sql(insert_file.data() + sizeof(INSERT_HASH_FILE) - 1, 258, path.data(), path.size());
    strcpy(insert_file.data() + end, SQL_QUARY_SCOPE_END);
    res = PQexec(dbconn, insert_file.data());
    exec_conn(res, PGRES_COMMAND_OK, "error: cann't insert file");
    PQclear(res);
    res = PQexec(dbconn, select_id.data());
    exec_conn(res, PGRES_TUPLES_OK, "error: cann't select file");
    if (PQntuples(res) < 1) {
      exit_error("error: cann't select file", 10);
    }
    result = PQgetvalue(res, 0, 0);
  }
  PQclear(res);
  return result;
}

bool check_valid_hash_filename(std::string filename)
{
  if ((filename.size() < sizeof(HASH_FILENAME_PREFIX) - 1 + HASH_FILENAME_POSTFIX_NUMBERS)
     || (memcmp(filename.data(), HASH_FILENAME_PREFIX, sizeof(HASH_FILENAME_PREFIX) - 1) != 0))
    return false;
  for (size_t i = sizeof(HASH_FILENAME_PREFIX) - 1; i < filename.size(); i++) {
    if (filename.data()[i] < '0' || filename.data()[i] > '9') return false;
  }
  return true;
}

std::string create_hash_filename_template(size_t numbers) {
  size_t postfix_begin = sizeof(HASH_FILENAME_PREFIX) - 1;
  std::string current_file(postfix_begin + numbers, 0);
  memcpy(current_file.data(), HASH_FILENAME_PREFIX, postfix_begin);
  strset(current_file.data() + postfix_begin, '0', numbers);
  return current_file;
}

void open_output_hash_file()
{
  auto last_file_pref = hashes_dir / PREF_LAST_HASH_FILENAME;
  std::string current_file;
  std::string buf(256, 0);
  bool find_file = false;
  deque_t<file_t>::iterator pref_file;
  if (std::filesystem::exists(last_file_pref)) {
    pref_file = openfile(last_file_pref.c_str(), O_RDWR);
    soft_assert(*pref_file);
    if (pref_file->to_end() < 256) {
      pref_file->to_begin();
      pref_file->read(buf.data(), 256);
      current_file = buf.data();
      auto last = hashes_dir / current_file;
      if (check_valid_hash_filename(current_file) && std::filesystem::exists(last)) {
        auto last_file = openfile(last, O_APPEND);
        if (last_file->to_end() < MAX_SINGLE_HASH_FILE_SIZE) {
          output_hash_file = last_file;
        } else {
          find_file = true;
          last_file.remove_element();
        }
      } else
        find_file = true;
    } else
      find_file = true;
  } else {
    pref_file = openfile(last_file_pref.c_str(), O_WRONLY | S_IRWXU | O_CREAT);
    soft_assert(*pref_file);
    find_file = true;
  }
  if (find_file) {
    if (!check_valid_hash_filename(current_file)) {
      current_file = create_hash_filename_template(HASH_FILENAME_POSTFIX_NUMBERS);
    }

    size_t postfix_begin = sizeof(HASH_FILENAME_PREFIX) - 1;
    size_t extra_numbers = 0;
    while (find_file) {
      soft_assert(current_file.data()[current_file.size()] == 0  &&
                  current_file.data()[current_file.size() - 1] >= '0' &&
                  current_file.data()[current_file.size() - 1] <= '9');
      for (size_t i = current_file.size() - 1; i >= postfix_begin && find_file; i--) {
        for (char* j = current_file.data() + i; *j <= '9'; (*j)++) {
          if (!std::filesystem::exists(hashes_dir / current_file)) {
            find_file = false;
            break;
          }
        }
      }
      if (find_file) {
        extra_numbers += 1;
        current_file = create_hash_filename_template(HASH_FILENAME_POSTFIX_NUMBERS + extra_numbers);
      }
    }
  }
  if (memcmp(buf.data(), current_file.data(), current_file.size() + 1) != 0) {
    auto trunc = pref_file->truncate();
    soft_assert(trunc == 0);
    pref_file->write(current_file.data(), current_file.size());
  }
  if (!output_hash_file) {
    soft_assert(check_valid_hash_filename(current_file));
    output_hash_file = openfile((hashes_dir / current_file).c_str(), O_WRONLY | O_CREAT | S_IRWXU);
    soft_assert(output_hash_file);
  }
  output_hash_id = select_or_insert_file(output_hash_file->path());
  pref_file.remove_element();
}

void close_fd(deque_t<file_t>::iterator it) {
  if (it) {
    it.remove_element();
  }
}

void init_hash_files() {
  PGresult* res = PQexec(dbconn, SELECT_FILES_FROM_DB);
  exec_conn(res, PGRES_TUPLES_OK, "error: can't query saved files from DB");
  size_t rows = PQntuples(res);
  if (rows > 0) {
    const int idcol   = PQfnumber(res, "id");
    const int pathcol = PQfnumber(res, "path");
    soft_assert(idcol > -1 && pathcol > -1);
    for (size_t i = 0; i < rows; i++) {
      auto [it, ok] = hashes_files.emplace(PQgetvalue(res, i, idcol),
                                           files.add({ PQgetvalue(res, i, pathcol), O_RDONLY }));
      soft_assert(ok);
    }
  }
  PQclear(res);
}

deque_t<file_t>::iterator open_hash_file(std::string id)
{
  auto it = hashes_files.find(id);
  soft_assert(it != hashes_files.end());
  if (it->second) {
    if (!*(it->second)) {
      if (opened_fd < max_fd) {
        it->second->open();
      } else {
        return files.end();
      }
    }
  } else {
    return files.end();
  }
  return it->second;
}

size_t save_buffer(const unsigned char * inbuf, size_t buflen) {
  size_t current;
  size_t hashing_bytes = HASHING_BLOCK_SIZE;
  size_t max = buflen / HASHING_BLOCK_SIZE;
  if ((buflen % HASHING_BLOCK_SIZE) > 0) max++;
  std::vector<unsigned char> hash_raw(max * BYTES_HASH);
  for (current = 0; current < max; current++) {
    const size_t delta = current * HASHING_BLOCK_SIZE;
    soft_assert(buflen - delta > 0);
    if (buflen - delta < HASHING_BLOCK_SIZE) {
      hashing_bytes = buflen - delta;
      soft_assert(current == max - 1);
    }
#if (HASH_BITS == 256)
    SHA256(inbuf + delta, hashing_bytes, hash_raw.data() + current * BYTES_HASH);
#elif
#error "unknown algoritm"
#endif
  }
  soft_assert(current == max);
  std::string hex(max * HASH_HEX_BYTES, 0);
  to_my_hex(hex.data(), hash_raw.data(), hash_raw.size());
  static const size_t first_part_end = sizeof(EXISTS_HASH) - 1;
  static bool request_init = false;
  static std::string find_request(first_part_end + HASH_HEX_BYTES + 3, 0);
  size_t insert_req_pos = sizeof(INSERT_MANY_CACHES) - 1;
  static std::string insert_request(insert_req_pos + INSERT_MAX_MANY_HASHES_LENGTH + 2, 0);
  if (!request_init) {
    memcpy(find_request.data(), EXISTS_HASH, first_part_end);
    memcpy(insert_request.data(), INSERT_MANY_CACHES, insert_req_pos);
    find_request.data()[find_request.size() - 1] = ';';
    request_init = true;
  }
#if (FULL_LOGGING)
  static bool request_printed = false;
  static bool insert_printed = false;
#endif
  std::string blocksize(BLOCK_SIZE_BYTES, 0);
  size_t bufpos = 0;
  hashing_bytes = HASHING_BLOCK_SIZE;
  size_t saving_hashes = 0;
  std::set<std::string_view> inserting_hashes;
  for (current = 0; current < max; current++) {
    soft_assert(requested_file->write((const char *) hash_raw.data() + current * BYTES_HASH, BYTES_HASH) == BYTES_HASH);
    if (inserting_hashes.count(std::string_view(hex.data() + HASH_HEX_BYTES * current, HASH_HEX_BYTES)) == 0) {
      inserting_hashes.insert(std::string_view(hex.data() + HASH_HEX_BYTES * current, HASH_HEX_BYTES));
      size_t added = add_wrapped_sql(find_request.data() + first_part_end, find_request.size() - first_part_end,
                                     hex.data() + HASH_HEX_BYTES * current, HASH_HEX_BYTES);
      soft_assert(added > 0);
#if (FULL_LOGGING)
      if (!request_printed) {
        std::cerr << "info: formed query: " << find_request.c_str() << std::endl;
        request_printed = true;
      }
#endif
      PGresult* res = PQexec(dbconn, find_request.c_str());
      exec_conn(res, ExecStatusType::PGRES_TUPLES_OK, "error: failed query hashes from DB");
      const size_t count = PQntuples(res);
      //bool cannt_find_block = false;
      if (buflen - bufpos < HASHING_BLOCK_SIZE) hashing_bytes = buflen - bufpos;
      if (count == 0) {
        for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++) {
          blocksize[i] = (hashing_bytes >> (8 * (BLOCK_SIZE_BYTES - i - 1))) % 256;
        }
        if (!output_hash_file) {
          exit_error("error: file struct destroyed", 10);
        }
        if (!(*output_hash_file)) {
          if (!(output_hash_file->open())) {
            exit_error(wrap_ostringstream("error: cann't open file " << output_hash_file->path()), 10);
          }
        }
        off_t writed_pos = output_hash_file->to_end();
        bool success_writing = output_hash_file->write(blocksize.data(), BLOCK_SIZE_BYTES) == BLOCK_SIZE_BYTES;
        success_writing = success_writing && (output_hash_file->write((const char *) inbuf + bufpos, hashing_bytes) == hashing_bytes);
        soft_assert(success_writing);
        if (saving_hashes > 0) insert_request.data()[insert_req_pos++] = ',';
        saving_hashes++;
        insert_request.data()[insert_req_pos++] = '(';
        insert_req_pos += add_wrapped_sql(insert_request.data() + insert_req_pos, insert_request.size() - insert_req_pos,
                                          hex.data() + HASH_HEX_BYTES * current, HASH_HEX_BYTES);
        insert_request.data()[insert_req_pos++] = ',';
        memcpy(insert_request.data() + insert_req_pos, output_hash_id.c_str(), output_hash_id.size());
        insert_req_pos += output_hash_id.size();
        insert_request.data()[insert_req_pos++] = ',';
        insert_req_pos += add_number(insert_request.data() + insert_req_pos, insert_request.size() - insert_req_pos, writed_pos);
        memcpy(insert_request.data() + insert_req_pos, INSERT_HASH_COUNT_END, sizeof(INSERT_HASH_COUNT_END) - 1);
        insert_req_pos += sizeof(INSERT_HASH_COUNT_END) - 1;
      }
      PQclear(res);
    }
    bufpos += hashing_bytes;
  }
  if (saving_hashes > 0) {
    insert_request.data()[insert_req_pos] = ';';
    insert_request.data()[insert_req_pos + 1] = 0;
#if (FULL_LOGGING)
    if (!insert_printed) {
      std::cerr << "insert request : " << insert_request.c_str() << std::endl;
      insert_printed = true;
    }
#endif
    PGresult* res = PQexec(dbconn, insert_request.c_str());
    exec_conn(res, ExecStatusType::PGRES_COMMAND_OK, "error: failed insert hashes into DB");
    PQclear(res);
  }
  return bufpos;
}

// returned filled buf size; nhashes - in max hashes in hashes arr, out - used hashes for buffer filling
size_t fill_buffer_from_hashes(char * buf, size_t bufsize, const char * hashes_arr, size_t *nhashes)
{
  soft_assert(buf && hashes_arr && nhashes);
  if (*nhashes == 0 || bufsize < (HASHING_BLOCK_SIZE))
    return 0;
  const size_t first_part_end = sizeof(SELECT_FILE_POS_FROM_HASHES) - 1;
  std::string find_request(first_part_end + HASH_HEX_BYTES + 3, 0);
  memcpy(find_request.data(), SELECT_FILE_POS_FROM_HASHES, first_part_end);
  find_request.data()[find_request.size() - 1] = ';';
#if (FULL_LOGGING)
  static bool request_printed = false;
#endif
  size_t outpos = 0;
  size_t all_hashes = 0;
  std::string hash_hex(HASH_HEX_BYTES, 0);
  std::vector<unsigned char> blocksize(BLOCK_SIZE_BYTES, 0);
  for (all_hashes = 0;
       (all_hashes < *nhashes) && (outpos + HASHING_BLOCK_SIZE <= bufsize); all_hashes++) {
    to_my_hex(hash_hex.data(), (unsigned char *) hashes_arr + all_hashes * BYTES_HASH, BYTES_HASH);
    add_wrapped_sql(find_request.data() + first_part_end, bufsize - first_part_end,
                    hash_hex.data(), HASH_HEX_BYTES);
#if (FULL_LOGGING)
    if (!request_printed) {
      std::cerr << "info: formed query: " << find_request << std::endl;
      request_printed = true;
    }
#endif
    PGresult* res = PQexec(dbconn, find_request.c_str());
    exec_conn(res, PGRES_TUPLES_OK, "error: failed query hashes from DB");
    bool cannt_find_block = false;
    if (PQntuples(res)) {
      const int file_col = PQfnumber(res, "file");
      const int pos_col  = PQfnumber(res, "pos");
      soft_assert(file_col > -1 && pos_col > -1);
      auto it = open_hash_file(PQgetvalue(res, 0, file_col));
      if (it) {
        size_t pos = atol(PQgetvalue(res, 0, pos_col));
        auto blocksize_readed = it->read(pos, (char *) blocksize.data(), BLOCK_SIZE_BYTES);
        soft_assert(blocksize_readed == BLOCK_SIZE_BYTES);
        size_t block_len = 0;
        for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++) {
          block_len += (blocksize.data()[i] << (8 * (BLOCK_SIZE_BYTES - i - 1)));
        }
#if (FULL_LOGGING)
        std::cerr << "block size: " << block_len << std::endl;
#endif
        auto readed = it->read(buf + outpos, block_len);
        if (readed < block_len) {
          std::cerr << "warn: reading block error, unreaded symbols replaced to \'x\'.\n";
          if (readed < 0) readed = 0;
          strset(buf + outpos + readed, 'x', block_len - readed);
        }
        outpos += block_len;
      } else {
        cannt_find_block = true;
      }
    } else {
      cannt_find_block = true;
    }
    PQclear(res);
    if (cannt_find_block) {
      std::cerr << "warn: block \'" << hash_hex << "\' not found, replace by \'x\' symbols\n";
      strset(buf + outpos, 'x', HASHING_BLOCK_SIZE);
      outpos += HASHING_BLOCK_SIZE;
    }
  }
#if (FULL_LOGGING)
  std::cerr << "hashes readed: " << all_hashes << std::endl;
#endif
  *nhashes = all_hashes;
  return outpos;
}

int main(int argc, char ** argv)
{
  if (argc < 2) {
    exit_error("command line args not found, aborted...", 1);
  }
  std::string filename;
  file_operation_t mode = NONE;
  for (int i = 1; i < argc; i++) {
    if (!(strcmp(argv[i], "-h") && strcmp(argv[i], "--help"))) {
      std::cout << "usage:"
                   "\n<program> (-h|--help) |"
                   "\n<program> -r filename |"
                   "\n<program> -w filename"
                   "\nuse option \"-h\" or \"--help\" for print this help."
                   "\nuse option \"-w\" for save data from stdin in storage with specified filename."
                   "\nuse option \"-r\" for read data from storage to stdout with specified filename."
                   "\nused params:"
                   "\n\tBUFFERED_READ_SIZE: "        << BUFFER_READ_SIZE
                << "\n\tHASHING_BLOCK_SIZE: "        << HASHING_BLOCK_SIZE
                << "\n\tSQL_REQUEST_LENGTH_LIMIT: "  << SQL_REQUEST_LENGTH_LIMIT
                << "\n\tINSERT_MANY_CACHES: "        << INSERT_MANY_HASHES_COUNT
                << " (max with current SQL_REQUEST_LENGTH_LIMIT: "
                << (SQL_REQUEST_LENGTH_LIMIT - sizeof(INSERT_MANY_CACHES) - 4) / INSERT_ROW_MAX_LENGTH
                << ")\n";
      return 0;
    }
    if (!strcmp(argv[i], "-r")) {
      if (mode != NONE) {
        exit_error("error: used some \"-w\" or \"-r\" parameters, aborted...", 4);
      }
      mode = READ;
    } else if (!strcmp(argv[i], "-w")) {
      if (mode != NONE) {
        exit_error("error: used some \"-w\" or \"-r\" parameters, aborted...", 4);
      }
      mode = WRITE;
    } else {
      if (filename.empty()) {
        filename = argv[i];
      } else {
        exit_error("error: too many filename parameters, aborted...", 3);
      }
    }
  }
  if (mode == NONE) {
    exit_error("no mode specified, aborted...\n", -3);
  }

  if (filename.empty()) {
    exit_error("error: filename not found in args, aborted...\n", 5);
  }

  files_dir = SUBDIRECTORY_FILES_PATH;
  if (!std::filesystem::exists(files_dir)) {
    if (!std::filesystem::create_directories(files_dir)) {
      exit_error(wrap_ostringstream("error: can't create directory \"" << files_dir << "\""), 9);
    }
  }

  hashes_dir = SUBDIRECTORY_HASHES_PATH;
  if (!std::filesystem::exists(hashes_dir)) {
    if (!std::filesystem::create_directories(hashes_dir)) {
      exit_error(wrap_ostringstream("error: can't create directory \"" << hashes_dir << "\""), 9);
    }
  }

  std::filesystem::path file = files_dir / filename;
  if (std::filesystem::exists(file)) {
    if (mode == WRITE) {
      exit_error("error: file exists, aborted...", 6);
    }
    if (filename.find_last_of("/") != std::string::npos) {
      std::filesystem::path sub_path_to_file =
          files_dir / filename.substr(0, filename.find_last_of("/"));
      if (!std::filesystem::exists(sub_path_to_file))
        std::filesystem::create_directories(sub_path_to_file);
    }
  } else {
    if (mode == READ) {
      exit_error("error: file not found, aborted...", 6);
    }
  }

  struct rlimit lim;
  getrlimit(RLIMIT_NOFILE, &lim);
  if (lim.rlim_cur > 100) {
    max_fd = lim.rlim_cur / 2;
  } else {
    max_fd = lim.rlim_max / 2;
  }

  if (max_fd < 50) {
    exit_error("error: fd limit is too low, aborted...", 11);
  }

  auto connection_info = openfile("db_connection.txt", O_RDONLY);
  std::string conninfo;
  if (!connection_info) {
    std::cerr << "error occurred while opening db_connection.txt, errno: " << errno << "\n";
    soft_close_all();
    return -1;
  }
  {
    std::string buffer(256, 0);
    while (connection_info->read(buffer.data(), 255)) {
      conninfo.append(buffer.data());
    }
  }
  connection_info.remove_element();
  dbconn = PQconnectdb(conninfo.data());
  if (PQstatus(dbconn) != CONNECTION_OK) {
    exit_error(wrap_ostringstream("Connection failed: " << PQerrorMessage(dbconn)
                                  << "\nstatus = " << PQstatus(dbconn)), -2);
  }
#if (!__RELEASE)
  PQsetNoticeProcessor(dbconn, noNoticeProcessor, nullptr);
#endif
  /*
  PGresult* res = PQexec(dbconn, "SET search_path = deduplication_server;");
  exec_conn(res, PGRES_COMMAND_OK, "SET failed: ");
  PQclear(res);
  */
  PGresult* res = PQexec(dbconn, CREATE_HASH_TABLE);
  exec_conn(res, PGRES_COMMAND_OK, "CREATE hash TABLE failed: ");
  PQclear(res);

  res = PQexec(dbconn, CREATE_FILE_TABLE);
  exec_conn(res, PGRES_COMMAND_OK, "CREATE file TABLE failed: ");
  PQclear(res);


  // reading mode
  if (mode == READ) {
    init_hash_files();
    requested_file = openfile(file.c_str(), O_RDONLY);
    soft_assert(*requested_file);
    const size_t buffer_hexes_size = ((int)(BUFFER_READ_SIZE / BYTES_HASH)) * BYTES_HASH;
    std::string readbuf(buffer_hexes_size, 0);
    std::string output(BUFFER_READ_SIZE, 0);
    off_t readed = 1;
    requested_file->to_begin();
    while (readed > 0) {
      readed = requested_file->read(readbuf.data(), buffer_hexes_size);
      soft_assert((readed % BYTES_HASH) == 0);
      size_t readed_hashes = readed / BYTES_HASH;
      size_t current_hashes = 0;
      while (current_hashes < readed_hashes) {
        size_t hashes_last = readed_hashes - current_hashes;
        size_t writed = fill_buffer_from_hashes(output.data(), BUFFER_READ_SIZE,
                                                readbuf.data() + current_hashes * BYTES_HASH, &hashes_last);
#if (FULL_LOGGING)
        std::cerr << "filled from hashes: " << writed << std::endl;
#endif
        soft_assert(writed > 0);
        current_hashes += hashes_last;
        std::cout.write(output.data(), writed);
      }
    }
  } else { // writing mode
    std::string readbuf(BUFFER_READ_SIZE, 0);
    open_output_hash_file();
    requested_file = openfile(file.c_str(), O_APPEND | O_WRONLY | O_CREAT | S_IRWXU);
    std::streamsize readed_bytes = 1;
    while (std::cin) {
      std::cin.read(readbuf.data(), BUFFER_READ_SIZE);
      readed_bytes = std::cin.gcount();
#if (FULL_LOGGING)
      std::cerr << "readed bytes " << readed_bytes << std::endl;
#endif
      if (readed_bytes > 0) {
        if (readed_bytes != save_buffer((const unsigned char *)readbuf.data(), readed_bytes))
          exit_error("error: saved len not equally buffer size", 10);
      } else
        break;
    }
  }
  soft_close_all();
  return 0;
}
