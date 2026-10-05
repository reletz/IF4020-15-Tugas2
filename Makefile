CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

TEST_BIN := test_roundtrip
TEST_UTIL_BIN := test_util
TEST_MAC_BIN := test_mac
SRC_DIR  := src
CORE_DIR := $(SRC_DIR)/core
UTIL_DIR := $(SRC_DIR)/util

.PHONY: test test-util test-mac clean

test:
	$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp tests/test_roundtrip.cpp -o $(TEST_BIN)
	./$(TEST_BIN)
	rm -f ./$(TEST_BIN)

test-util:
	$(CXX) $(CXXFLAGS) -Itests $(UTIL_DIR)/*.cpp tests/test_util.cpp -o $(TEST_UTIL_BIN)
	./$(TEST_UTIL_BIN)
	rm -f ./$(TEST_UTIL_BIN)

test-mac:
	$(CXX) $(CXXFLAGS) -Itests $(CORE_DIR)/*.cpp $(UTIL_DIR)/*.cpp src/ops/mac.cpp tests/test_mac.cpp -o $(TEST_MAC_BIN)
	./$(TEST_MAC_BIN)
	rm -f ./$(TEST_MAC_BIN)

clean:
	rm -f $(TEST_BIN) $(TEST_UTIL_BIN) $(TEST_MAC_BIN)
