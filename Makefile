CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude
OPTFLAGS := -O2

TEST_BIN    := test_roundtrip
GEN_BIN     := gen_sbox_tool
ANALYZE_BIN := analyze_sbox_tool
SRC_DIR     := src
CORE_DIR    := $(SRC_DIR)/core

.PHONY: test sbox analyze clean

test:
	$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp tests/test_roundtrip.cpp -o $(TEST_BIN)
	./$(TEST_BIN)
	rm -f $(TEST_BIN)

sbox: tools/gen_sbox.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) tools/gen_sbox.cpp -o $(GEN_BIN)
	./$(GEN_BIN) > $(CORE_DIR)/sbox_table.inc
	rm -f $(GEN_BIN) $(GEN_BIN).exe

analyze: $(CORE_DIR)/sbox.cpp $(CORE_DIR)/sbox_table.inc tools/analyze_sbox.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(CORE_DIR)/sbox.cpp tools/analyze_sbox.cpp -o $(ANALYZE_BIN)
	./$(ANALYZE_BIN)
	rm -f $(ANALYZE_BIN) $(ANALYZE_BIN).exe

clean:
	rm -f $(TEST_BIN) $(GEN_BIN) $(ANALYZE_BIN) *.exe
