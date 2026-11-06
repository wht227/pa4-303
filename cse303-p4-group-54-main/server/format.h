/// format.h defines the file format used by the server.

/// In p4, whenever a change is made to either table, a small log entry
/// must be added to the file, describing the change.  There are five commands
/// that can cause a change: REG, SET, KVI, KVU, and KVD.  Note that REG can
/// simply append an AUTH entry, KVI can simply append a KVKV entry, and
/// a KVU that results in an insert can also use KVKV.  This means that we
/// only require new entries for SET, KVD, and a KVU that results in an update.
/// The formats that correspond to these entries are defined below:
///
/// New Profile File entry format:
/// - 4-byte constant AUTHDIFF = "AUP_"  (defined in constants.h)
/// - 4-byte binary write of the length of the username
/// - 4-byte binary write of length of the profile file
/// - Binary write of the bytes of the username
/// - Binary write of the profile file
/// - Binary write of some bytes of padding, to ensure that the next entry will
///   be aligned on an 4-byte boundary.
///
/// Updated Key/Value entry format:
/// - 4-byte constant KVUPDATE = "KVU_" (defined in constants.h)
/// - 4-byte binary write of the length of the key
/// - 4-byte binary write of the length of the value
/// - Binary write of the bytes of the key
/// - Binary write of the bytes of the value
/// - Binary write of some bytes of padding, to ensure that the next entry will
///   be aligned on an 4-byte boundary.
///
/// Delete Key/Value entry format;
/// - 4-byte constant KVDELETE = "KVD_" (defined in constants.h)
/// - 4-byte binary write of the length of the key
/// - Binary write of the bytes of the key
/// - Binary write of some bytes of padding, to ensure that the next entry will
///   be aligned on an 4-byte boundary.
///
/// Note that a SAV operation will never generate these entries, and is still
/// supported.  SAV is, in effect, a compacting operation that removes data that
/// is logically invalid but not yet erased from the file.
///
/// Note that it will be necessary to keep the log file open at all times in
/// p4, except possibly for a brief period during the handling of a SAV
/// command.
///
/// Note, too, that whenever an incremental update is logged to file, it will be
/// important to use the fflush() and fsync() operations to ensure that the
/// changes make it to disk before a response is given to the client.