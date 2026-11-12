# Build a client and server using the student's files

# The executables will have the suffix .exe
EXESUFFIX = exe

# Names for building the client:
CLIENT_MAIN     = client
CLIENT_CXX      =
CLIENT_COMMON   =
CLIENT_PROVIDED = client requests crypto err file net my_crypto

# Names for building the server
SERVER_MAIN     = server
SERVER_CXX      = my_storage persist
SERVER_COMMON   =
SERVER_PROVIDED = server responses parsing crypto err file net my_pool \
                  my_crypto concurrentmap_factories

# NB: This Makefile does not add extra CXXFLAGS

# Pull in the common build rules
include common.mk
