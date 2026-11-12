# Configure g++:
# - Default to 64 bits, but allow overriding on the command line
# - Use CXXEXTRA and LDEXTRA to allow additional flags
BITS     ?= 64
CXX       = g++
LD        = g++
CXXFLAGS  = -MMD -O3 -m$(BITS) -ggdb -std=c++17 -Wall -Werror -fPIC $(CXXEXTRA) -I/usr/include/openssl3
LDFLAGS   = -m$(BITS) -lpthread -l:libcrypto.so.3 -ldl $(LDEXTRA)
SOFLAGS   = -fPIC -shared

# Hard-coded name of the solutions folder
SDIR = solutions

# Give a name to the output folder, and ensure it is created before any
# compilation happens
ODIR      := ./obj$(BITS)
OUTFOLDER := $(shell mkdir -p $(ODIR))

# Names of all the .o files needed to create the client executable
CLIENT_O  = $(patsubst %, $(ODIR)/%.o, $(CLIENT_CXX))
CLIENT_O += $(patsubst %, $(ODIR)/%.o, $(CLIENT_COMMON))
CLIENT_O += $(patsubst %, $(SDIR)/%.o, $(CLIENT_PROVIDED))

# Names of all the .o files needed to create the server executable
SERVER_O  = $(patsubst %, $(ODIR)/%.o, $(SERVER_CXX))
SERVER_O += $(patsubst %, $(ODIR)/%.o, $(SERVER_COMMON))
SERVER_O += $(patsubst %, $(SDIR)/%.o, $(SERVER_PROVIDED))

# Names of all the .o files needed to create the shared objects
#
# NB: shared object builds require special management of .o files in the common
#     folder, so we need two names instead of one
SO_BUILD_O   = $(patsubst %, $(ODIR)/%.o, $(SO_CXX))
SO_COMMON_O += $(patsubst %, $(ODIR)/%.o, $(SO_COMMON))

# Names of all the .o, .exe, and .so files to build
OFILES   = $(CLIENT_O) $(SERVER_O) $(BENCH_O) $(SO_BUILD_O) $(SO_COMMON_O)
EXEFILES = $(patsubst %, $(ODIR)/%.$(EXESUFFIX), $(CLIENT_MAIN) $(SERVER_MAIN) $(BENCH_MAIN))
SOFILES  = $(patsubst %, $(ODIR)/%.so, $(SO_CXX))

# Names of all .d files, so we can get dependencies right
DFILES     = $(patsubst %.o, %.d, $(OFILES))

# Build 'all' by default, and don't clobber .o files after each build
.DEFAULT_GOAL = all
.PRECIOUS: $(OFILES)
.PHONY: all clean

# Typing 'make' should build all the .exe and .so files
all: $(EXEFILES) $(SOFILES)

# Typing 'make clean' should clean up by removing $(OUTFOLDER)
clean:
	@echo Cleaning up...
	@rm -rf $(ODIR)

# Rules for building .o files
$(ODIR)/%.o: client/%.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)
$(ODIR)/%.o: server/%.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)
$(ODIR)/%.o: common/%.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)
$(ODIR)/%.o: bench/%.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)
$(ODIR)/%.o: so/%.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)

# Rules for building executables
$(ODIR)/$(CLIENT_MAIN).$(EXESUFFIX): $(CLIENT_O)
	@echo "[LD] $^ --> $@"
	@$(CXX) $^ -o $@ $(LDFLAGS)
$(ODIR)/$(SERVER_MAIN).$(EXESUFFIX): $(SERVER_O)
	@echo "[LD] $^ --> $@"
	@$(CXX) $^ -o $@ $(LDFLAGS)
$(ODIR)/$(BENCH_MAIN).$(EXESUFFIX): $(BENCH_O)
	@echo "[LD] $^ --> $@"
	@$(CXX) $^ -o $@ $(LDFLAGS)

# Rules for building .so files
$(ODIR)/%.so: $(ODIR)/%.o $(SO_COMMON_O)
	@echo "[LD] $^ --> $@"
	@$(CXX) $^ -o $@ $(CXXFLAGS) $(SOFLAGS)

# Include any dependencies we generated previously
-include $(DFILES)
