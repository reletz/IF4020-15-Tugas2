CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude -Itests
OPTFLAGS := -O2

OMP ?= 1
ifeq ($(OMP),1)
    CXXFLAGS += -fopenmp
endif

SRC_DIR   := src
CORE_DIR  := $(SRC_DIR)/core
BUILD_DIR := build
APP_NAME  := cipher_cli
SBOX_INC  := $(CORE_DIR)/sbox_table.inc
BENCH_CSV ?= docs/report/data/bench.csv
RESULTS_DIR ?= test-results

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
    RM      = if exist $(call fixpath,$1) del /F /Q $(call fixpath,$1)
else
    fixpath = $1
    MKDIR   = mkdir -p $@
    RMDIR   = rm -rf $(BUILD_DIR)
    MV      = mv -f $1 $2
    run     = ./$1
    RM      = rm -f $1
endif

.PHONY: all app test avalanche sac bench bench-csv sbox analyze mode-analysis rounds results docs clean $(RUN_TESTS) test-util test-mac test-container test-e2e test-d

all: app

$(BUILD_DIR):
	$(MKDIR)

app: $(BUILD_DIR)/$(APP_NAME)$(EXE)
	@cp -f $(BUILD_DIR)/$(APP_NAME)$(EXE) $(APP_NAME)$(EXE) 2>/dev/null || :

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

$(BUILD_DIR)/bench_modes$(EXE): analysis/bench_modes.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

bench: $(BUILD_DIR)/bench_modes$(EXE)
	@$(call run,$<) $(BENCH_ARGS)

bench-csv: $(BUILD_DIR)/bench_modes$(EXE)
	@mkdir -p $(dir $(BENCH_CSV))
	$(call run,$<) --csv $(BENCH_ARGS) > $(BENCH_CSV)
	@echo Results saved to $(BENCH_CSV)

sac: $(BUILD_DIR)/sac$(EXE)
	@$(call run,$<)

$(BUILD_DIR)/mode_analysis$(EXE): analysis/mode_analysis.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

mode-analysis: $(BUILD_DIR)/mode_analysis$(EXE)
	@$(call run,$<) --out $(RESULTS_DIR)/data $(ANALYSIS_ARGS)

$(BUILD_DIR)/rounds$(EXE): analysis/rounds.cpp $(LIB_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(LIB_SRCS) $< -o $@

rounds: $(BUILD_DIR)/rounds$(EXE)
	@$(call run,$<) $(ROUNDS_ARGS)

$(BUILD_DIR)/gen_sbox$(EXE): tools/gen_sbox.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $< -o $@

sbox: $(BUILD_DIR)/gen_sbox$(EXE)
	$(call run,$<) > $(BUILD_DIR)/sbox_table.inc.tmp
	$(call MV,$(BUILD_DIR)/sbox_table.inc.tmp,$(SBOX_INC))

$(BUILD_DIR)/analyze_sbox$(EXE): $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp $(SBOX_INC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp -o $@

analyze: $(BUILD_DIR)/analyze_sbox$(EXE)
	@$(call run,$<)

test-util: run-test_util
test-mac: run-test_mac
test-container: run-test_container

test-e2e: app
	CLI="$(BUILD_DIR)/$(APP_NAME)$(EXE)" bash tests/e2e.sh

test-d: test-util test-mac test-container test-e2e

# Jalankan semua tes dan analisis, simpan log/CSV/grafik ke $(RESULTS_DIR)/
results: app $(BUILD_DIR)/mode_analysis$(EXE) $(BUILD_DIR)/rounds$(EXE) $(BUILD_DIR)/avalanche$(EXE) $(BUILD_DIR)/sac$(EXE) $(BUILD_DIR)/analyze_sbox$(EXE)
	OUT="$(RESULTS_DIR)" bash tests/run_all.sh

# API documentation (Doxygen) ke build/html
docs:
	doxygen Doxyfile

clean:
	-$(RMDIR)
	-$(call RM,$(APP_NAME)$(EXE))
