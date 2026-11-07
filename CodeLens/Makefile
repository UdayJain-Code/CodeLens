CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SRC      := src/file_loader.cpp src/lexer.cpp src/normalizer.cpp src/fingerprint.cpp \
            src/similarity.cpp src/report.cpp src/pipeline.cpp src/menu.cpp src/cli.cpp
CORE     := $(filter-out src/menu.cpp src/cli.cpp,$(SRC))

all: codelens

codelens: $(SRC) src/main.cpp
	$(CXX) $(CXXFLAGS) $(SRC) src/main.cpp -o $@

codelens_tests: $(CORE) tests/test_main.cpp
	$(CXX) $(CXXFLAGS) -Isrc $(CORE) tests/test_main.cpp -o $@

test: codelens_tests
	./codelens_tests

demo: codelens
	./codelens --dir sample

clean:
	rm -f codelens codelens_tests codelens.exe codelens_tests.exe

.PHONY: all test demo clean
