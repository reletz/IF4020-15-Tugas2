CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

TEST_BIN := test_roundtrip
TEST_UTIL_BIN := test_util
SRC_DIR  := src
CORE_DIR := $(SRC_DIR)/core
UTIL_DIR := $(SRC_DIR)/util

.PHONY: test test-util clean

test:
	$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp tests/test_roundtrip.cpp -o $(TEST_BIN)
	./$(TEST_BIN)
	rm -f ./$(TEST_BIN)

test-util:
	$(CXX) $(CXXFLAGS) -Itests $(UTIL_DIR)/*.cpp tests/test_util.cpp -o $(TEST_UTIL_BIN)
	./$(TEST_UTIL_BIN)
	rm -f ./$(TEST_UTIL_BIN)

clean:
	rm -f $(TEST_BIN) $(TEST_UTIL_BIN)
