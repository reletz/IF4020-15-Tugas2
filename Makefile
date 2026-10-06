CXX      ?= g++
<<<<<<< HEAD
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude
=======
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude -Itests
>>>>>>> 9543fe6 (feat:merge makefile:)
OPTFLAGS := -O2

SRC_DIR   := src
CORE_DIR  := $(SRC_DIR)/core
BUILD_DIR := build
APP_NAME  := cipher_cli

LIB_SRCS   := $(filter-out $(SRC_DIR)/main.cpp,$(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp))
TEST_NAMES := $(basename $(notdir $(wildcard tests/test_*.cpp)))
RUN_TESTS  := $(addprefix run-,$(TEST_NAMES))

ifeq ($(OS),Windows_NT)
    EXE := .exe
    ifneq ($(findstring cmd,$(SHELL)),)
        WIN_CMD := 1
    endif
else
    EXE :=
endif

ifdef WIN_CMD
    fixpath = $(subst /,\,$1)
    MKDIR   = if not exist $(call fixpath,$@) mkdir $(call fixpath,$@)
    RMDIR   = if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
    MV      = move /Y $(call fixpath,$(BUILD_DIR)/sbox_table.inc.tmp) $(call fixpath,$(CORE_DIR)/sbox_table.inc) > nul
    run     = $(call fixpath,$1)
else
    fixpath = $1
    MKDIR   = mkdir -p $@
    RMDIR   = rm -rf $(BUILD_DIR)
    MV      = mv -f $(BUILD_DIR)/sbox_table.inc.tmp $(CORE_DIR)/sbox_table.inc
    run     = ./$1
endif

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

all: app

$(BUILD_DIR):
	$(MKDIR)

app: $(BUILD_DIR)/$(APP_NAME)$(EXE)

$(BUILD_DIR)/$(APP_NAME)$(EXE): $(SRC_DIR)/main.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

test: $(RUN_TESTS)

$(BUILD_DIR)/test_%$(EXE): tests/test_%.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LIB_SRCS) $< -o $@

$(RUN_TESTS): run-%: $(BUILD_DIR)/%$(EXE)
	@echo == $*
	@$(call run,$<)

ifneq ($(wildcard analysis/avalanche.cpp),)
.PHONY: avalanche
$(BUILD_DIR)/avalanche$(EXE): analysis/avalanche.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@
avalanche: $(BUILD_DIR)/avalanche$(EXE)
	@$(call run,$<)
endif

ifneq ($(wildcard analysis/sac.cpp),)
.PHONY: sac
$(BUILD_DIR)/sac$(EXE): analysis/sac.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@
sac: $(BUILD_DIR)/sac$(EXE)
	@$(call run,$<)
endif

ifneq ($(wildcard tools/gen_sbox.cpp),)
.PHONY: sbox
$(BUILD_DIR)/gen_sbox$(EXE): tools/gen_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $< -o $@
sbox: $(BUILD_DIR)/gen_sbox$(EXE)
	$(call run,$<) > $(BUILD_DIR)/sbox_table.inc.tmp
	$(MV)
endif

ifneq ($(wildcard tools/analyze_sbox.cpp),)
.PHONY: analyze
$(BUILD_DIR)/analyze_sbox$(EXE): $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $^ -o $@
analyze: $(BUILD_DIR)/analyze_sbox$(EXE)
	@$(call run,$<)
endif

clean:
	-$(RMDIR)
