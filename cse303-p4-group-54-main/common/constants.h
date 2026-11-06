/// This file defines the constants used throughout the program.  For details
/// about the message and file formats that use these constants, please see the
/// README file for this and previous assignments.

#pragma once

#include <openssl/sha.h>
#include <string>

////////////////////////////////////////////////////////////////
// Below are the definitions needed for Assignments #1 and #2
////////////////////////////////////////////////////////////////

/// Maximum length of a user name
const int LEN_UNAME = 32;

/// Maximum length of a user's actual password
const int LEN_PASSWORD = 32;

/// Maximum length of a hashed password
const int LEN_PASSHASH = SHA256_DIGEST_LENGTH;

/// Maximum length of a user's profile file
const int LEN_PROFILE_FILE = 1048576;

/// Max length of a block before RSA encryption
const int LEN_RBLOCK_CONTENT = 128;

/// Length of a block after RSA encryption
const int LEN_RKBLOCK = 256;

/// Length of an RSA public key
const int LEN_RSA_PUBKEY = 451; // NB: Defined by OpenSSL

/// Length of salt
const int LEN_SALT = 16;

/// size of an RSA key
const int RSA_KEYSIZE = 2048;

/// size of an AES key
const int AES_KEYSIZE = 32;

/// size of an AES initialization vector
const int AES_IVSIZE = 16;

/// Request the server's public key
const std::string REQ_KEY = "KEY_";

/// Request the creation of a new user
const std::string REQ_REG = "REG_";

/// Force the server to stop
const std::string REQ_BYE = "EXIT";

/// Force the server to send all its data to disk
const std::string REQ_SAV = "SAVE";

/// Allow a user to set their profile content
const std::string REQ_SET = "SETP";

/// Allow a user to request a user's profile content
const std::string REQ_GET = "GETP";

/// Get all user names
const std::string REQ_ALL = "ALL_";

/// Response code to indicate that the command was successful
const std::string RES_OK = "OK__";

/// Response code to indicate that the registered user already exists
const std::string RES_ERR_USER_EXISTS = "ERR_USER_EXISTS";

/// Response code to indicate that the client gave a bad username or password
const std::string RES_ERR_LOGIN = "ERR_LOGIN";

/// Response code to indicate that the client request was improperly formatted
const std::string RES_ERR_REQ_FMT = "ERR_REQ_FMT";

/// Response code to indicate that there is no data to send back
const std::string RES_ERR_NO_DATA = "ERR_NO_DATA";

/// Response code to indicate that the user being looked up is invalid
const std::string RES_ERR_NO_USER = "ERR_NO_USER";

/// Response code to indicate that the requested command doesn't exist
const std::string RES_ERR_INV_CMD = "ERR_INVALID_COMMAND";

/// Response code to indicate that the client didn't get as much data as
/// expected
const std::string RES_ERR_XMIT = "ERR_XMIT";

/// Response code to indicate that the client data can't be decrypted with the
/// provided AES key
const std::string RES_ERR_CRYPTO = "ERR_CRYPTO";

/// Response code to indicate that the server had an internal error, such as a
/// bad read from a file, error creating a salt, or failure to fork()
const std::string RES_ERR_SERVER = "ERR_SERVER";

/// Response code to indicate that something has not been implemented
const std::string RES_ERR_UNIMPLEMENTED = "ERR_UNIMPLEMENTED";

/// A unique 4-byte code to use as a prefix each time an AuthTable Entry is
/// written to disk.
const std::string AUTHENTRY = "AUTH";

////////////////////////////////////////////////////////////////
// Below are the additions for Assignment #3
////////////////////////////////////////////////////////////////

/// Maximum length of a key in the key-value store
const int LEN_KEY = 1024;

/// Maximum length of a value in the key-value store
const int LEN_VAL = 1048576;

/// Insert a new key/value pair
const std::string REQ_KVI = "KVI_";

/// Get the value for a given key
const std::string REQ_KVG = "KVG_";

/// Delete a key/value pair
const std::string REQ_KVD = "KVD_";

/// Upsert a key/value pair
const std::string REQ_KVU = "KVU_";

/// Get a newline-delimited list of all keys
const std::string REQ_KVA = "KVA_";

/// Response code to indicate that an upsert did an insert
const std::string RES_OKINS = "OK_INSERT";

/// Response code to indicate that an upsert did an update
const std::string RES_OKUPD = "OK_UPDATE";

/// Response code to indicate an error when searching for a key
const std::string RES_ERR_KEY = "ERR_KEY";

/// A unique 4-byte code to use as a prefix each time a KV pair is written to
/// disk.
const std::string KVENTRY = "KVKV";

////////////////////////////////////////////////////////////////
// Below are the additions for Assignment #4
////////////////////////////////////////////////////////////////

/// A unique 4-byte code for incremental persistence of changes to the auth
/// table
const std::string AUTHDIFF = "AUP_";

/// A unique 4-byte code for incremental persistence of updates to the kv store
const std::string KVUPDATE = "KVU_";

/// A unique 4-byte code for incremental persistence of deletes to the kv store
const std::string KVDELETE = "KVD_";