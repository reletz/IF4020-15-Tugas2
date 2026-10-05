CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude
OPTFLAGS := -O2

TEST_BIN := test_roundtrip
TEST_UTIL_BIN := test_util
TEST_MAC_BIN := test_mac
TEST_CONTAINER_BIN := test_container
SRC_DIR  := src
CORE_DIR := $(SRC_DIR)/core
UTIL_DIR := $(SRC_DIR)/util

APP_BIN  := cipher_cli

.PHONY: app test test-util test-mac test-container clean

app:
	$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp $(UTIL_DIR)/*.cpp src/ops/mac.cpp src/ops/container.cpp src/main.cpp -o $(APP_BIN)

ifeq ($(OS),Windows_NT)
    EXE := .exe
    ifneq ($(findstring cmd,$(SHELL)),)
        fixpath = $(subst /,\,$1)
        MKDIR   = if not exist $(call fixpath,$@) mkdir $(call fixpath,$@)
        RMDIR   = if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
        MV      = move /Y $(call fixpath,$(BUILD_DIR)/sbox_table.inc.tmp) $(call fixpath,$(CORE_DIR)/sbox_table.inc) > nul
    else
        fixpath = $1
        MKDIR   = mkdir -p $@
        RMDIR   = rm -rf $(BUILD_DIR)
        MV      = mv -f $(BUILD_DIR)/sbox_table.inc.tmp $(CORE_DIR)/sbox_table.inc
    endif
else
    EXE     :=
    fixpath = $1
    MKDIR   = mkdir -p $@
    RMDIR   = rm -rf $(BUILD_DIR)
    MV      = mv -f $(BUILD_DIR)/sbox_table.inc.tmp $(CORE_DIR)/sbox_table.inc
endif

ifeq ($(OS)$(findstring cmd,$(SHELL)),Windows_NTcmd)
    run = $(call fixpath,$1)
else
    run = ./$1
endif

.PHONY: all test avalanche sac sbox analyze clean

all: test

$(BUILD_DIR):
	$(MKDIR)

$(BUILD_DIR)/%$(EXE): tests/%.cpp $(CORE_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SRCS) $< -o $@

RUN_TESTS := $(addprefix run-,$(TESTS))

.PHONY: $(RUN_TESTS)

test: $(RUN_TESTS)

$(RUN_TESTS): run-%: $(BUILD_DIR)/%$(EXE)
	@echo == $*
	@$(call run,$<)

$(BUILD_DIR)/avalanche$(EXE): analysis/avalanche.cpp $(CORE_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_SRCS) $< -o $@

$(BUILD_DIR)/sac$(EXE): analysis/sac.cpp $(CORE_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_SRCS) $< -o $@

avalanche: $(BUILD_DIR)/avalanche$(EXE)
	@$(call run,$<)

sac: $(BUILD_DIR)/sac$(EXE)
	@$(call run,$<)

$(BUILD_DIR)/gen_sbox$(EXE): tools/gen_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $< -o $@

sbox: $(BUILD_DIR)/gen_sbox$(EXE)
	$(call run,$<) > $(BUILD_DIR)/sbox_table.inc.tmp
	$(MV)

$(BUILD_DIR)/analyze_sbox$(EXE): $(CORE_DIR)/sbox.cpp $(CORE_DIR)/sbox_table.inc tools/analyze_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp -o $@

analyze: $(BUILD_DIR)/analyze_sbox$(EXE)
	@$(call run,$<)

test-util:
	$(CXX) $(CXXFLAGS) -Itests $(UTIL_DIR)/*.cpp tests/test_util.cpp -o $(TEST_UTIL_BIN)
	./$(TEST_UTIL_BIN)
	rm -f ./$(TEST_UTIL_BIN)

test-mac:
	$(CXX) $(CXXFLAGS) -Itests $(CORE_DIR)/*.cpp $(UTIL_DIR)/*.cpp src/ops/mac.cpp tests/test_mac.cpp -o $(TEST_MAC_BIN)
	./$(TEST_MAC_BIN)
	rm -f ./$(TEST_MAC_BIN)

test-container:
	$(CXX) $(CXXFLAGS) -Itests $(CORE_DIR)/*.cpp $(UTIL_DIR)/*.cpp src/ops/mac.cpp src/ops/container.cpp tests/test_container.cpp -o $(TEST_CONTAINER_BIN)
	./$(TEST_CONTAINER_BIN)
	rm -f ./$(TEST_CONTAINER_BIN)

clean:
	rm -f $(TEST_BIN) $(TEST_UTIL_BIN) $(TEST_MAC_BIN) $(TEST_CONTAINER_BIN) $(APP_BIN)
