CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

TEST_BIN := test_roundtrip

.PHONY: test clean

test:
	$(CXX) $(CXXFLAGS) src/core/cipher.cpp tests/test_roundtrip.cpp -o $(TEST_BIN)
	./$(TEST_BIN)
	rm -f ./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)
