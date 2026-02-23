CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread
SRC_DIR  := .

# ─── Targets ──────────────────────────────────────────────────────────────────

.PHONY: all clean run bench

all: chronostore chronostore_bench

chronostore: main.cpp store.cpp store.h lru.h ttl_manager.h persistence.h \
             command_parser.h threadpool.h
	$(CXX) $(CXXFLAGS) main.cpp store.cpp -o $@

chronostore_bench: benchmark.cpp store.cpp store.h lru.h ttl_manager.h \
                   persistence.h threadpool.h
	$(CXX) $(CXXFLAGS) benchmark.cpp store.cpp -o $@

run: chronostore
	./chronostore

bench: chronostore_bench
	./chronostore_bench

clean:
	del /Q chronostore.exe chronostore_bench.exe snapshot.bin 2>nul || \
	rm -f chronostore chronostore_bench snapshot.bin
