#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../common/constants.h"
#include "../common/contextmanager.h"
#include "../common/err.h"

#include "authtableentry.h"
#include "map.h"
#include "map_factories.h"
#include "persist.h"
#include "storage.h"

using namespace std;

/// Helper function to append a 4-byte little-endian unsigned integer to a buffer
static void append_uint32(vector<uint8_t> &buf, uint32_t val) {
  uint8_t *p = (uint8_t *)&val;
  buf.insert(buf.end(), p, p + 4);
}

/// Helper function to compute SHA-256 hash of password and salt using OpenSSL EVP API.
/// This prevents compiler deprecation warnings with OpenSSL 3.0.
static vector<uint8_t> hash_pass(const string &pass, const vector<uint8_t> &salt) {
  // Combine password bytes and salt bytes
  vector<uint8_t> combined(pass.begin(), pass.end());
  combined.insert(combined.end(), salt.begin(), salt.end());

  vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, combined.data(), combined.size());
  EVP_DigestFinal_ex(ctx, hash.data(), nullptr);
  EVP_MD_CTX_free(ctx);

  return hash;
}

/// MyStorage is the student implementation of the Storage class
class MyStorage : public Storage {
  /// The map of authentication information, indexed by username
  Map<string, AuthTableEntry> *auth_table;

  /// The map of key/value pairs
  Map<string, vector<uint8_t>> *kv_store;

  /// The name of the file from which the Storage object was loaded, and to
  /// which we persist the Storage object every time it changes
  const string filename;

  /// The open file for appending incremental updates
  FILE *storage_file = nullptr;

public:
  /// Construct an empty object and specify the file from which it should be
  /// loaded. To avoid exceptions and errors in the constructor, the act of
  /// loading data is separate from construction.
  ///
  /// @param fname   The name of the file to use for persistence
  /// @param buckets The number of buckets in the hash table
  MyStorage(const std::string &fname, size_t buckets)
      : auth_table(authtable_factory(buckets)),
        kv_store(kvstore_factory(buckets)), filename(fname) {}

  /// Destructor for the storage object.
  virtual ~MyStorage() {
    shutdown();
    delete auth_table;
    delete kv_store;
  }

  /// Create a new entry in the Auth table. If the user already exists, return
  /// an error. Otherwise, create a salt, hash the password, and then save an
  /// entry with the username, salt, hashed password, and a zero-byte content.
  ///
  /// @param user The user name to register
  /// @param pass The password to associate with that user name
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t add_user(const string &user, const string &pass) {
    // 1. Generate random salt
    vector<uint8_t> salt(LEN_SALT);
    if (RAND_bytes(salt.data(), LEN_SALT) != 1) {
      cout << "Error in RAND_bytes()\n";
      return {false, RES_ERR_SERVER, {}};
    }

    // 2. Hash password with salt
    vector<uint8_t> pass_hash = hash_pass(pass, salt);

    // 3. Create entry with empty content
    AuthTableEntry entry{user, salt, pass_hash, {}};

    // 4. Atomically insert into auth_table and log to file inside on_success callback
    bool inserted = auth_table->insert(user, entry, [&]() {
      log_svv0(storage_file, AUTHENTRY, user, salt, pass_hash);
    });

    if (!inserted) {
      return {false, RES_ERR_USER_EXISTS, {}};
    }
    return {true, RES_OK, {}};
  }

  /// Set the data bytes for a user, but do so if and only if the password
  /// matches
  ///
  /// @param user    The name of the user whose content is being set
  /// @param pass    The password for the user, used to authenticate
  /// @param content The data to set for this user
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t set_user_data(const string &user, const string &pass,
                                 const vector<uint8_t> &content) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    // Atomically update user content and log AUP_ diff to file
    auth_table->do_with(user, [&](AuthTableEntry &entry) {
      entry.content = content;
      log_sv(storage_file, AUTHDIFF, user, content);
    });

    return {true, RES_OK, {}};
  }

  /// Return a copy of the user data for a user, but do so only if the password
  /// matches
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param who  The name of the user whose content is being fetched
  ///
  /// @return A result tuple, as described in storage.h. Note that "no data" is
  ///         an error
  virtual result_t get_user_data(const string &user, const string &pass,
                                 const string &who) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    vector<uint8_t> result_data;
    bool found = false;
    bool has_data = false;

    auth_table->do_with_readonly(who, [&](const AuthTableEntry &entry) {
      found = true;
      if (!entry.content.empty()) {
        has_data = true;
        result_data = entry.content;
      }
    });

    if (!found) {
      return {false, RES_ERR_NO_USER, {}};
    }
    if (!has_data) {
      return {false, RES_ERR_NO_DATA, {}};
    }
    return {true, RES_OK, result_data};
  }

  /// Return a newline-delimited string containing all of the usernames in the
  /// auth table
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t get_all_users(const string &user, const string &pass) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    string users_str = "";
    auth_table->do_all_readonly(
        [&](const string &uname, const AuthTableEntry &entry) {
          users_str += uname + "\n";
        },
        []() {});

    return {true, RES_OK, vector<uint8_t>(users_str.begin(), users_str.end())};
  }

  /// Authenticate a user
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t auth(const string &user, const string &pass) {
    bool authenticated = false;
    bool found = auth_table->do_with_readonly(user, [&](const AuthTableEntry &entry) {
      if (hash_pass(pass, entry.salt) == entry.pass_hash) {
        authenticated = true;
      }
    });

    if (!found || !authenticated) {
      return {false, RES_ERR_LOGIN, {}};
    }
    return {true, RES_OK, {}};
  }

  /// Create a new key/value mapping in the table
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose mapping is being created
  /// @param val  The value to copy into the map
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_insert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    bool inserted = kv_store->insert(key, val, [&]() {
      log_sv(storage_file, KVENTRY, key, val);
    });

    if (!inserted) {
      return {false, RES_ERR_KEY, {}};
    }
    return {true, RES_OK, {}};
  }

  /// Get a copy of the value to which a key is mapped
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose value is being fetched
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_get(const string &user, const string &pass,
                          const string &key) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    vector<uint8_t> result_val;
    bool found = kv_store->do_with_readonly(key, [&](const vector<uint8_t> &v) {
      result_val = v;
    });

    if (!found) {
      return {false, RES_ERR_KEY, {}};
    }
    return {true, RES_OK, result_val};
  }

  /// Delete a key/value mapping
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose value is being deleted
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_delete(const string &user, const string &pass,
                             const string &key) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    bool removed = kv_store->remove(key, [&]() {
      log_s(storage_file, KVDELETE, key);
    });

    if (!removed) {
      return {false, RES_ERR_KEY, {}};
    }
    return {true, RES_OK, {}};
  }

  /// Insert or update, so that the given key is mapped to the give value
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  /// @param key  The key whose mapping is being upserted
  /// @param val  The value to copy into the map
  ///
  /// @return A result tuple, as described in storage.h. Note that there are
  /// two "OK" messages, depending on whether we get an insert or an update.
  virtual result_t kv_upsert(const string &user, const string &pass,
                             const string &key, const vector<uint8_t> &val) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    bool is_insert = kv_store->upsert(
        key, val,
        [&]() {
          log_sv(storage_file, KVENTRY, key, val);
        },
        [&]() {
          log_sv(storage_file, KVUPDATE, key, val);
        });

    if (is_insert) {
      return {true, RES_OKINS, {}};
    } else {
      return {true, RES_OKUPD, {}};
    }
  }

  /// Return all of the keys in the kv_store, as a "\n"-delimited string
  ///
  /// @param user The name of the user who made the request
  /// @param pass The password for the user, used to authenticate
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t kv_all(const string &user, const string &pass) {
    // First authenticate user
    auto auth_res = auth(user, pass);
    if (!auth_res.succeeded) {
      return auth_res;
    }

    string keys_str = "";
    kv_store->do_all_readonly(
        [&](const string &k, const vector<uint8_t> &v) {
          keys_str += k + "\n";
        },
        []() {});

    if (keys_str.empty()) {
      return {false, RES_ERR_NO_DATA, {}};
    }
    return {true, RES_OK, vector<uint8_t>(keys_str.begin(), keys_str.end())};
  }

  /// Shut down the storage when the server stops. This method needs to close
  /// any open files related to incremental persistence. It also needs to clean
  /// up any state related to .so files. This is only called when all threads
  /// have stopped accessing the Storage object.
  virtual void shutdown() {
    if (storage_file != nullptr) {
      fclose(storage_file);
      storage_file = nullptr;
    }
  }

  /// Write the entire Storage object to the file specified by this.filename. To
  /// ensure durability, Storage must be persisted in two steps. First, it must
  /// be written to a temporary file (this.filename.tmp). Then the temporary
  /// file can be renamed to replace the older version of the Storage object.
  ///
  /// @return A result tuple, as described in storage.h
  virtual result_t save_file() {
    string tmp_name = filename + ".tmp";
    FILE *tmp = fopen(tmp_name.c_str(), "wb");
    if (tmp == nullptr) {
      return {false, "Unable to create temporary file", {}};
    }

    bool success = true;

    // Use 2-Phase Locking: hold locks across both auth_table and kv_store
    auth_table->do_all_readonly(
        [&](const string &user, const AuthTableEntry &entry) {
          // Write AUTH entry to tmp file
          uint32_t ulen = entry.username.size();
          uint32_t slen = entry.salt.size();
          uint32_t plen = entry.pass_hash.size();
          uint32_t clen = entry.content.size();

          size_t total = 4 + 4 + 4 + 4 + 4 + ulen + slen + plen + clen;
          size_t pad = (4 - (total % 4)) % 4;

          vector<uint8_t> buf;
          buf.reserve(total + pad);

          buf.insert(buf.end(), AUTHENTRY.begin(), AUTHENTRY.end());

          // Write all 4 lengths first
          append_uint32(buf, ulen);
          append_uint32(buf, slen);
          append_uint32(buf, plen);
          append_uint32(buf, clen);

          // Write payload bytes
          buf.insert(buf.end(), entry.username.begin(), entry.username.end());
          buf.insert(buf.end(), entry.salt.begin(), entry.salt.end());
          buf.insert(buf.end(), entry.pass_hash.begin(), entry.pass_hash.end());
          buf.insert(buf.end(), entry.content.begin(), entry.content.end());

          buf.insert(buf.end(), pad, 0);

          fwrite(buf.data(), sizeof(uint8_t), buf.size(), tmp);
        },
        [&]() {
          // Inside the 'then' of auth_table, acquire locks on kv_store
          kv_store->do_all_readonly(
              [&](const string &key, const vector<uint8_t> &val) {
                // Write KVKV entry to tmp file
                uint32_t klen = key.size();
                uint32_t vlen = val.size();

                size_t total = 4 + 4 + 4 + klen + vlen;
                size_t pad = (4 - (total % 4)) % 4;

                vector<uint8_t> buf;
                buf.reserve(total + pad);

                buf.insert(buf.end(), KVENTRY.begin(), KVENTRY.end());

                append_uint32(buf, klen);
                append_uint32(buf, vlen);

                buf.insert(buf.end(), key.begin(), key.end());
                buf.insert(buf.end(), val.begin(), val.end());

                buf.insert(buf.end(), pad, 0);

                fwrite(buf.data(), sizeof(uint8_t), buf.size(), tmp);
              },
              [&]() {
                // Both auth_table and kv_store locks are held here simultaneously
                fclose(tmp);
                if (storage_file != nullptr) {
                  fclose(storage_file);
                  storage_file = nullptr;
                }

                if (rename(tmp_name.c_str(), filename.c_str()) != 0) {
                  cout << "Error renaming file after persisting\n";
                  success = false;
                  return;
                }

                storage_file = fopen(filename.c_str(), "a");
                if (storage_file == nullptr) {
                  cout << "Unable to re-open " << filename << " after persisting\n";
                  success = false;
                  return;
                }
              });
        });

    if (!success) {
      return {false, RES_ERR_SERVER, {}};
    }
    return {true, RES_OK, {}};
  }

  /// Populate the Storage object by loading this.filename. Note that load()
  /// begins by clearing the maps, so that when the call is complete, exactly
  /// and only the contents of the file are in the Storage object.
  ///
  /// @return A result tuple, as described in storage.h. Note that a
  ///         non-existent file is not an error.
  virtual result_t load_file() {
    auth_table->clear();
    kv_store->clear();

    storage_file = fopen(filename.c_str(), "r");
    if (storage_file == nullptr) {
      storage_file = fopen(filename.c_str(), "a");
      return {true, "File not found: " + filename, {}};
    }

    char header[5] = {0};
    while (fread(header, 1, 4, storage_file) == 4) {
      string tag(header, 4);

      if (tag == AUTHENTRY) {
        uint32_t ulen = 0;
        if (fread(&ulen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthTable uname len", {}};
        }
        uint32_t slen = 0;
        if (fread(&slen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthTable salt len", {}};
        }
        uint32_t plen = 0;
        if (fread(&plen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthTable pass len", {}};
        }
        uint32_t clen = 0;
        if (fread(&clen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthTable content len", {}};
        }

        string uname(ulen, '\0');
        if (fread(&uname[0], 1, ulen, storage_file) != ulen) {
          return {false, "Error reading AuthTable uname", {}};
        }

        vector<uint8_t> salt(slen);
        if (fread(salt.data(), 1, slen, storage_file) != slen) {
          return {false, "Error reading AuthTable salt", {}};
        }

        vector<uint8_t> pass_hash(plen);
        if (fread(pass_hash.data(), 1, plen, storage_file) != plen) {
          return {false, "Error reading AuthTable pass", {}};
        }

        vector<uint8_t> content(clen);
        if (clen > 0) {
          if (fread(content.data(), 1, clen, storage_file) != clen) {
            return {false, "Error reading AuthTable content", {}};
          }
        }

        size_t total = 4 + 4 + 4 + 4 + 4 + ulen + slen + plen + clen;
        size_t pad = (4 - (total % 4)) % 4;
        if (pad > 0) {
          uint8_t pad_buf[4];
          if (fread(pad_buf, 1, pad, storage_file) != pad) {
            return {false, "Error reading AuthTable padding", {}};
          }
        }

        AuthTableEntry entry{uname, salt, pass_hash, content};
        if (!auth_table->insert(uname, entry, []() {})) {
          return {false, "Unable to insert from file into auth table", {}};
        }
      } else if (tag == KVENTRY) {
        uint32_t klen = 0;
        if (fread(&klen, 4, 1, storage_file) != 1) {
          return {false, "Error reading KVStore key len", {}};
        }
        uint32_t vlen = 0;
        if (fread(&vlen, 4, 1, storage_file) != 1) {
          return {false, "Error reading KVStore val len", {}};
        }
        string key(klen, '\0');
        if (fread(&key[0], 1, klen, storage_file) != klen) {
          return {false, "Error reading KVStore key", {}};
        }
        vector<uint8_t> val(vlen);
        if (vlen > 0) {
          if (fread(val.data(), 1, vlen, storage_file) != vlen) {
            return {false, "Error reading KVStore val", {}};
          }
        }

        size_t total = 4 + 4 + 4 + klen + vlen;
        size_t pad = (4 - (total % 4)) % 4;
        if (pad > 0) {
          uint8_t pad_buf[4];
          if (fread(pad_buf, 1, pad, storage_file) != pad) {
            return {false, "Error reading KVStore padding", {}};
          }
        }

        if (!kv_store->insert(key, val, []() {})) {
          return {false, "Unable to insert from file into kv store", {}};
        }
      } else if (tag == AUTHDIFF) {
        uint32_t ulen = 0;
        if (fread(&ulen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthDiff uname len", {}};
        }
        uint32_t clen = 0;
        if (fread(&clen, 4, 1, storage_file) != 1) {
          return {false, "Error reading AuthDiff content len", {}};
        }
        string uname(ulen, '\0');
        if (fread(&uname[0], 1, ulen, storage_file) != ulen) {
          return {false, "Error reading AuthDiff uname", {}};
        }
        vector<uint8_t> content(clen);
        if (clen > 0) {
          if (fread(content.data(), 1, clen, storage_file) != clen) {
            return {false, "Error reading AuthDiff content", {}};
          }
        }

        size_t total = 4 + 4 + 4 + ulen + clen;
        size_t pad = (4 - (total % 4)) % 4;
        if (pad > 0) {
          uint8_t pad_buf[4];
          if (fread(pad_buf, 1, pad, storage_file) != pad) {
            return {false, "Error reading AuthDiff padding", {}};
          }
        }

        bool updated = auth_table->do_with(uname, [&](AuthTableEntry &entry) {
          entry.content = content;
        });
        if (!updated) {
          return {false, "Unable to diff from file into auth table", {}};
        }
      } else if (tag == KVUPDATE) {
        uint32_t klen = 0;
        if (fread(&klen, 4, 1, storage_file) != 1) {
          return {false, "Error reading KVUpdate key len", {}};
        }
        uint32_t vlen = 0;
        if (fread(&vlen, 4, 1, storage_file) != 1) {
          return {false, "Error reading KVUpdate val len", {}};
        }
        string key(klen, '\0');
        if (fread(&key[0], 1, klen, storage_file) != klen) {
          return {false, "Error reading KVUpdate key", {}};
        }
        vector<uint8_t> val(vlen);
        if (vlen > 0) {
          if (fread(val.data(), 1, vlen, storage_file) != vlen) {
            return {false, "Error reading KVUpdate value", {}};
          }
        }

        size_t total = 4 + 4 + 4 + klen + vlen;
        size_t pad = (4 - (total % 4)) % 4;
        if (pad > 0) {
          uint8_t pad_buf[4];
          if (fread(pad_buf, 1, pad, storage_file) != pad) {
            return {false, "Error reading KVUpdate padding", {}};
          }
        }

        bool updated = kv_store->do_with(key, [&](vector<uint8_t> &v) {
          v = val;
        });
        if (!updated) {
          return {false, "Unable to update from file into KV store", {}};
        }
      } else if (tag == KVDELETE) {
        uint32_t klen = 0;
        if (fread(&klen, 4, 1, storage_file) != 1) {
          return {false, "Error reading KVDelete key len", {}};
        }
        string key(klen, '\0');
        if (fread(&key[0], 1, klen, storage_file) != klen) {
          return {false, "Error reading KVDelete key", {}};
        }

        size_t total = 4 + 4 + klen;
        size_t pad = (4 - (total % 4)) % 4;
        if (pad > 0) {
          uint8_t pad_buf[4];
          if (fread(pad_buf, 1, pad, storage_file) != pad) {
            return {false, "Error reading KVDelete padding", {}};
          }
        }

        if (!kv_store->remove(key, []() {})) {
          return {false, "Unable to delete from file into KV store", {}};
        }
      } else {
        return {false, "Unrecognized packet type in file", {}};
      }
    }

    // Close the read handle and reopen in append mode so future operations write to file
    fclose(storage_file);
    storage_file = fopen(filename.c_str(), "a");

    return {true, "Loaded: " + filename, {}};
  }
};

/// Create an empty Storage object and specify the file from which it should be
/// loaded. To avoid exceptions and errors in the constructor, the act of
/// loading data is separate from construction.
///
/// @param fname   The name of the file to use for persistence
/// @param buckets The number of buckets in the hash table
Storage *storage_factory(const std::string &fname, size_t buckets) {
  return new MyStorage(fname, buckets);
}
