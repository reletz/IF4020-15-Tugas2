CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

TEST_BIN := test_roundtrip
SRC_DIR  := src
CORE_DIR := $(SRC_DIR)/core

.PHONY: test clean

test:
	$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp tests/test_roundtrip.cpp -o $(TEST_BIN)
	./$(TEST_BIN)
	rm -f ./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)
