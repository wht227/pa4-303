#!/usr/bin/python3
import cse303

# Configure constants and users
cse303.indentation = 80
cse303.verbose = cse303.check_args_verbose()
alice = cse303.UserConfig("alice", "alice_is_awesome")
afile1 = "solutions/err.o"
afile2 = "common/err.h"
allfile = "allfile"
k1 = "k1"
k1file1 = "solutions/net.o"
k1file2 = "server/server.cc"
k2 = "second_key"
k2file1 = "server/parsing.h"
k2file2 = "server/storage.h"
k3 = "third_key"
k3file1 = "solutions/file.o"
k3file2 = "common/pool.h"

# Create objects with server and client configuration
server = cse303.ServerConfig(
    "./obj64/server.exe", "9999", "rsa", "company.dir", "4", "1024"
)
client = cse303.ClientConfig("./obj64/client.exe", "localhost", "9999", "localhost.pub")

# Check if we should use solution server or client
cse303.override_exe(server, client)

# Set up a clean slate before getting started
cse303.line()
print("Getting ready to run tests")
cse303.line()
cse303.clean_common_files(server, client)  # .pub, .pri, .dir files
cse303.killprocs()

print()
cse303.line()
print("Test #1: REG should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Starting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Generating RSA keys as ("
        + server.keyfile
        + ".pub, "
        + server.keyfile
        + ".pri)",
        "File not found: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Registering new user alice.", "OK__", client.reg(alice), server)
cse303.after(
    server.pid
)  # need an extra cleanup to handle the KEY that was sent by first REG
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
expect_size1 = cse303.next4(4 + 4 + len(alice.name) + 4 + 16 + 4 + 32 + 4)
cse303.verify_filesize(server.dirfile, expect_size1)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd(
    "Registering new user alice.", "ERR_USER_EXISTS", client.reg(alice), server
)
cse303.do_cmd(
    "Checking alice's content.", "ERR_NO_DATA", client.getC(alice, alice.name), server
)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #2: SET should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Setting alice's content.", "OK__", client.setC(alice, afile1), server)
expect_size2 = cse303.next4(
    expect_size1 + 4 + 4 + len("alice") + 4 + cse303.get_len(afile1)
)
cse303.verify_filesize(server.dirfile, expect_size2)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd(
    "Checking alice's content.", "OK__", client.getC(alice, alice.name), server
)
cse303.check_file_result(afile1, alice.name)
cse303.do_cmd("Re-setting alice's content.", "OK__", client.setC(alice, afile2), server)
expect_size3 = cse303.next4(
    expect_size2 + 4 + 4 + len("alice") + 4 + cse303.get_len(afile2)
)
cse303.verify_filesize(server.dirfile, expect_size3)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #3: KVI should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Setting key k1.", "OK__", client.kvI(alice, k1, k1file1), server)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
expect_size4 = cse303.next4(
    expect_size3 + 4 + 4 + len(k1) + 4 + cse303.get_len(k1file1)
)
cse303.verify_filesize(server.dirfile, expect_size4)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Checking key k1.", "OK__", client.kvG(alice, k1), server)
cse303.check_file_result(k1file1, k1)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #4: KVU should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Upserting key k1.", "OK_UPDATE", client.kvU(alice, k1, k1file2), server)
cse303.do_cmd("Upserting key k2.", "OK_INSERT", client.kvU(alice, k2, k2file1), server)
expect_size5 = cse303.next4(
    expect_size4
    + 4
    + 4
    + len(k1)
    + 4
    + cse303.get_len(k1file2)
    + 4
    + 4
    + len(k2)
    + 4
    + cse303.get_len(k2file1)
)
cse303.verify_filesize(server.dirfile, expect_size5)
cse303.verify_peek(server.dirfile, expect_size4, 4, "KVU_")
cse303.verify_peek(
    server.dirfile,
    expect_size4 + 4 + cse303.next4(4 + len(k1) + 4 + cse303.get_len(k1file2)),
    4,
    "KVKV",
)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Checking key k1.", "OK__", client.kvG(alice, k1), server)
cse303.check_file_result(k1file2, k1)
cse303.do_cmd("Checking key k2.", "OK__", client.kvG(alice, k2), server)
cse303.check_file_result(k2file1, k2)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #5: KVD should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Deleting key k1.", "OK__", client.kvD(alice, k1), server)
expect_size6 = cse303.next4(expect_size5 + 4 + 4 + len(k1))
cse303.verify_filesize(server.dirfile, expect_size6)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Checking key k1.", "ERR_KEY", client.kvG(alice, k1), server)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #6: Additional KVU should persist without SAV")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Upserting key k2.", "OK_UPDATE", client.kvU(alice, k2, k2file1), server)
cse303.do_cmd("Upserting key k2.", "OK_UPDATE", client.kvU(alice, k2, k2file2), server)
cse303.do_cmd("Upserting key k3.", "OK_INSERT", client.kvU(alice, k3, k3file1), server)
cse303.do_cmd("Upserting key k3.", "OK_UPDATE", client.kvU(alice, k3, k3file2), server)
expect_size7 = (
    expect_size6
    + cse303.next4(4 + 4 + len(k2) + 4 + cse303.get_len(k2file1))
    + cse303.next4(4 + 4 + len(k2) + 4 + cse303.get_len(k2file2))
    + cse303.next4(4 + 4 + len(k3) + 4 + cse303.get_len(k3file1))
    + cse303.next4(4 + 4 + len(k3) + 4 + cse303.get_len(k3file2))
)
cse303.verify_filesize(server.dirfile, expect_size7)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd("Checking key k2.", "OK__", client.kvG(alice, k2), server)
cse303.check_file_result(k2file2, k2)
cse303.do_cmd("Checking key k3.", "OK__", client.kvG(alice, k3), server)
cse303.check_file_result(k3file2, k3)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

print()
cse303.line()
print("Test #7: SAV should compact the directory")
cse303.line()
server.pid = cse303.do_cmd_a(
    "Restarting server:",
    [
        "Listening on port "
        + server.port
        + " using (key/data) = (rsa, "
        + server.dirfile
        + ")",
        "Loaded: " + server.dirfile,
    ],
    server.launchcmd(),
)
cse303.waitfor(2)
cse303.do_cmd(
    "Instructing server to persist data.", "OK__", client.persist(alice), server
)
expect_size8 = (
    cse303.next4(4 + 4 + len(alice.name) + 4 + 16 + 4 + 32 + 4 + cse303.get_len(afile2))
    + cse303.next4(4 + 4 + len(k2) + 4 + cse303.get_len(k2file2))
    + cse303.next4(4 + 4 + len(k3) + 4 + cse303.get_len(k3file2))
)
cse303.verify_filesize(server.dirfile, expect_size8)
cse303.do_cmd(
    "Checking alice's content.", "OK__", client.getC(alice, alice.name), server
)
cse303.check_file_result(afile2, alice.name)
cse303.do_cmd("Checking key k1.", "ERR_KEY", client.kvG(alice, k1), server)
cse303.do_cmd("Checking key k2.", "OK__", client.kvG(alice, k2), server)
cse303.check_file_result(k2file2, k2)
cse303.do_cmd("Checking key k3.", "OK__", client.kvG(alice, k3), server)
cse303.check_file_result(k3file2, k3)
cse303.do_cmd("Stopping server.", "OK__", client.bye(alice), server)
cse303.await_server("Waiting for server to shut down.", "Server terminated", server)

cse303.clean_common_files(server, client)

print()
