#include <cstdint>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

#include "persist.h"

using namespace std;

/// Helper function to append a 4-byte little-endian unsigned integer to a buffer
static void append_uint32(vector<uint8_t> &buf, uint32_t val) {
  uint8_t *p = (uint8_t *)&val;
  buf.insert(buf.end(), p, p + 4);
}

/// Atomically add an incremental update message to the open file
///
/// This variant puts a delimiter, a string, two vectors, and a zero into the
/// file. (Used for registering a new user)
///
/// Format:
/// - 4-byte delimiter "AUTH"
/// - 4-byte length of username
/// - 4-byte length of salt
/// - 4-byte length of pass_hash
/// - 4-byte length of content (0)
/// - username bytes
/// - salt bytes
/// - pass_hash bytes
/// - padding bytes (aligned to 4-byte boundary)
///
/// @param logfile The file to write into
/// @param delim   The 4-byte string that starts the log entry
/// @param s       The string to add to the message (username)
/// @param v1      The first vector to add to the message (salt)
/// @param v2      The second vector to add to the message (password hash)
void log_svv0(FILE *logfile, const string &delim, const string &s,
              const vector<uint8_t> &v1, const vector<uint8_t> &v2) {
  if (logfile == nullptr) {
    return;
  }

  uint32_t len_s = s.size();
  uint32_t len_v1 = v1.size();
  uint32_t len_v2 = v2.size();
  uint32_t len_zero = 0;

  // Calculate total payload size:
  // 4 (delim) + 4 (len_s) + 4 (len_v1) + 4 (len_v2) + 4 (0) + len_s + len_v1 + len_v2
  size_t total = 4 + 4 + 4 + 4 + 4 + len_s + len_v1 + len_v2;
  size_t pad = (4 - (total % 4)) % 4;

  vector<uint8_t> buf;
  buf.reserve(total + pad);

  // 1. Delimiter ("AUTH")
  buf.insert(buf.end(), delim.begin(), delim.end());

  // 2. All 4 lengths first
  append_uint32(buf, len_s);
  append_uint32(buf, len_v1);
  append_uint32(buf, len_v2);
  append_uint32(buf, len_zero);

  // 3. Payload contents
  buf.insert(buf.end(), s.begin(), s.end());
  buf.insert(buf.end(), v1.begin(), v1.end());
  buf.insert(buf.end(), v2.begin(), v2.end());

  // 4. Padding bytes
  buf.insert(buf.end(), pad, 0);

  // Write atomically in a single fwrite, flush, and fsync to disk
  fwrite(buf.data(), sizeof(uint8_t), buf.size(), logfile);
  fflush(logfile);
  fsync(fileno(logfile));
}

/// Atomically add an incremental update message to the open file
///
/// This variant puts a delimiter, a string, and a vector into the file.
/// (Used for profile updates "AUP_", key inserts "KVKV", and key updates "KVU_")
///
/// Format:
/// - 4-byte delimiter
/// - 4-byte length of string
/// - 4-byte length of vector
/// - string bytes
/// - vector bytes
/// - padding bytes
///
/// @param logfile The file to write into
/// @param delim   The 4-byte string that starts the log entry
/// @param s1      The string to add to the message
/// @param v1      The vector to add to the message
void log_sv(FILE *logfile, const string &delim, const string &s1,
            const vector<uint8_t> &v1) {
  if (logfile == nullptr) {
    return;
  }

  uint32_t len_s = s1.size();
  uint32_t len_v = v1.size();

  // Format: delim (4) + len_s (4) + len_v (4) + len_s + len_v
  size_t total = 4 + 4 + 4 + len_s + len_v;
  size_t pad = (4 - (total % 4)) % 4;

  vector<uint8_t> buf;
  buf.reserve(total + pad);

  // 1. Delimiter
  buf.insert(buf.end(), delim.begin(), delim.end());

  // 2. Length of string and length of vector
  append_uint32(buf, len_s);
  append_uint32(buf, len_v);

  // 3. Bytes of string and vector
  buf.insert(buf.end(), s1.begin(), s1.end());
  buf.insert(buf.end(), v1.begin(), v1.end());

  // 4. Padding bytes
  buf.insert(buf.end(), pad, 0);

  // Write atomically in a single fwrite, flush, and fsync
  fwrite(buf.data(), sizeof(uint8_t), buf.size(), logfile);
  fflush(logfile);
  fsync(fileno(logfile));
}

/// Atomically add an incremental update message to the open file
///
/// This variant puts a delimiter and a string into the file.
/// (Used for key deletes "KVD_")
///
/// Format:
/// - 4-byte delimiter
/// - 4-byte length of string
/// - string bytes
/// - padding bytes
///
/// @param logfile The file to write into
/// @param delim   The 4-byte string that starts the log entry
/// @param s1      The string to add to the message (key to delete)
void log_s(FILE *logfile, const string &delim, const string &s1) {
  if (logfile == nullptr) {
    return;
  }

  uint32_t len_s = s1.size();

  // Format: delim (4) + len_s (4) + len_s
  size_t total = 4 + 4 + len_s;
  size_t pad = (4 - (total % 4)) % 4;

  vector<uint8_t> buf;
  buf.reserve(total + pad);

  // 1. Delimiter
  buf.insert(buf.end(), delim.begin(), delim.end());

  // 2. Length of key
  append_uint32(buf, len_s);

  // 3. Bytes of key
  buf.insert(buf.end(), s1.begin(), s1.end());

  // 4. Padding bytes
  buf.insert(buf.end(), pad, 0);

  // Write atomically in a single fwrite, flush, and fsync
  fwrite(buf.data(), sizeof(uint8_t), buf.size(), logfile);
  fflush(logfile);
  fsync(fileno(logfile));
}
