CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude

SRC_DIR  := src
CORE_DIR := $(SRC_DIR)/core
TESTS    := test_roundtrip test_permutation

.PHONY: test clean

test:
	@for t in $(TESTS); do \
		echo "== $$t"; \
		$(CXX) $(CXXFLAGS) $(CORE_DIR)/*.cpp tests/$$t.cpp -o $$t || exit 1; \
		./$$t || { rm -f $$t; exit 1; }; \
		rm -f $$t; \
	done

clean:
	rm -f $(TESTS)
