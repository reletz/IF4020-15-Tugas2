CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

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

test-container:
	$(CXX) $(CXXFLAGS) -Itests $(CORE_DIR)/*.cpp $(UTIL_DIR)/*.cpp src/ops/mac.cpp src/ops/container.cpp tests/test_container.cpp -o $(TEST_CONTAINER_BIN)
	./$(TEST_CONTAINER_BIN)
	rm -f ./$(TEST_CONTAINER_BIN)

clean:
	rm -f $(TEST_BIN) $(TEST_UTIL_BIN) $(TEST_MAC_BIN) $(TEST_CONTAINER_BIN) $(APP_BIN)
