CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude -Itests
OPTFLAGS := -O2

SRC_DIR   := src
CORE_DIR  := $(SRC_DIR)/core
BUILD_DIR := build
APP_NAME  := cipher_cli
SBOX_INC  := $(CORE_DIR)/sbox_table.inc

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
    MV      = move /Y $(call fixpath,$1) $(call fixpath,$2) > nul
    run     = $(call fixpath,$1)
else
    fixpath = $1
    MKDIR   = mkdir -p $@
    RMDIR   = rm -rf $(BUILD_DIR)
    MV      = mv -f $1 $2
    run     = ./$1
endif

.PHONY: all app test avalanche sac sbox analyze clean $(RUN_TESTS)

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

$(BUILD_DIR)/avalanche$(EXE): analysis/avalanche.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

avalanche: $(BUILD_DIR)/avalanche$(EXE)
	@$(call run,$<)

$(BUILD_DIR)/sac$(EXE): analysis/sac.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

sac: $(BUILD_DIR)/sac$(EXE)
	@$(call run,$<)

$(BUILD_DIR)/gen_sbox$(EXE): tools/gen_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $< -o $@

sbox: $(BUILD_DIR)/gen_sbox$(EXE)
	$(call run,$<) > $(BUILD_DIR)/sbox_table.inc.tmp
	$(call MV,$(BUILD_DIR)/sbox_table.inc.tmp,$(SBOX_INC))

$(BUILD_DIR)/analyze_sbox$(EXE): $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp $(SBOX_INC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp -o $@

analyze: $(BUILD_DIR)/analyze_sbox$(EXE)
	@$(call run,$<)

clean:
	-$(RMDIR)
