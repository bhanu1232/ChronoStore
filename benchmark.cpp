/**
 * benchmark.cpp — ChronoStore Throughput Benchmark
 *
 * Measures:
 *   1. Sequential WRITE: 100,000 SET ops
 *   2. Sequential READ : 100,000 GET ops (all hits)
 *   3. Random    READ : 100,000 GET ops (random keys, ~50% hit rate)
 *   4. Mixed     R/W  : 100,000 ops (70% GET, 30% SET)
 *
 * Compile:
 *   g++ -std=c++17 -O2 -pthread benchmark.cpp store.cpp -o chronostore_bench
 *
 * Run:
 *   ./chronostore_bench
 */
#include "store.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using hrc = std::chrono::high_resolution_clock;

// ─── Formatting helpers ───────────────────────────────────────────────────────

static void printHeader(const std::string& title) {
    std::cout << "\n\033[1;36m  ==============================================\033[0m\n";
    std::cout << "  \033[1m" << title << "\033[0m\n";
    std::cout << "\033[1;36m  ==============================================\033[0m\n";
}

static void printResult(const std::string& label, size_t ops,
                        std::chrono::nanoseconds duration) {
    double secs    = duration.count() / 1e9;
    double ops_sec = static_cast<double>(ops) / secs;
    double ns_op   = static_cast<double>(duration.count()) / static_cast<double>(ops);

    std::cout << std::fixed;
    std::cout << "  \033[32m" << std::setw(18) << std::left << label << "\033[0m"
              << "  \033[33m" << std::setw(10) << std::right << std::setprecision(0)
              << ops_sec << " ops/s\033[0m"
              << "  \033[90m(" << std::setprecision(1) << ns_op << " ns/op"
              << "  total: " << std::setprecision(3) << secs << "s)\033[0m\n";
}

// ─── Benchmark cases ─────────────────────────────────────────────────────────

static constexpr size_t N           = 100'000;
static constexpr size_t BENCH_CAP   = 200'000; // large enough to avoid eviction in write test

int main() {
    std::cout << "\033[1;35m\n";
    std::cout << "   ██████╗ ███████╗███╗   ██╗ ██████╗██╗  ██╗\n";
    std::cout << "   ██╔══██╗██╔════╝████╗  ██║██╔════╝██║  ██║\n";
    std::cout << "   ██████╔╝█████╗  ██╔██╗ ██║██║     ███████║\n";
    std::cout << "   ██╔══██╗██╔══╝  ██║╚██╗██║██║     ██╔══██║\n";
    std::cout << "   ██████╔╝███████╗██║ ╚████║╚██████╗██║  ██║\n";
    std::cout << "   ╚═════╝ ╚══════╝╚═╝  ╚═══╝ ╚═════╝╚═╝  ╚═╝\n";
    std::cout << "\033[0m";
    std::cout << "  ChronoStore Throughput Benchmark — " << N / 1000 << "k ops per phase\n";

    // ── 1. WRITE benchmark ────────────────────────────────────────────────────
    printHeader("Phase 1: Sequential WRITE (SET)");

    KVStore write_store(BENCH_CAP);
    {
        auto   start = hrc::now();
        for (size_t i = 0; i < N; ++i) {
            write_store.set("key:" + std::to_string(i), "value:" + std::to_string(i));
        }
        auto dur = hrc::now() - start;
        printResult("Sequential SET", N, dur);
    }

    // ── 2. SEQUENTIAL READ (all hits) ─────────────────────────────────────────
    printHeader("Phase 2: Sequential READ (all hits)");
    {
        auto start = hrc::now();
        size_t hits = 0;
        for (size_t i = 0; i < N; ++i) {
            if (write_store.get("key:" + std::to_string(i))) ++hits;
        }
        auto dur = hrc::now() - start;
        printResult("Sequential GET", N, dur);
        std::cout << "  \033[90m  → " << hits << "/" << N << " hits\033[0m\n";
    }

    // ── 3. RANDOM READ (~50% hit) ─────────────────────────────────────────────
    printHeader("Phase 3: Random READ (~50% hit rate)");
    {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, N * 2 - 1); // keys 0..(2N-1)

        auto   start = hrc::now();
        size_t hits  = 0;
        for (size_t i = 0; i < N; ++i) {
            if (write_store.get("key:" + std::to_string(dist(rng)))) ++hits;
        }
        auto dur = hrc::now() - start;
        printResult("Random GET", N, dur);
        double hr = 100.0 * hits / N;
        std::cout << "  \033[90m  → " << std::fixed << std::setprecision(1)
                  << hr << "% hit rate\033[0m\n";
    }

    // ── 4. MIXED READ/WRITE (70/30) ───────────────────────────────────────────
    printHeader("Phase 4: Mixed R/W (70% GET, 30% SET)");
    {
        std::mt19937 rng2(123);
        std::uniform_int_distribution<size_t> key_dist(0, N - 1);
        std::uniform_int_distribution<int>    op_dist(1, 10);

        auto   start = hrc::now();
        for (size_t i = 0; i < N; ++i) {
            std::string k = "key:" + std::to_string(key_dist(rng2));
            if (op_dist(rng2) <= 7) {
                write_store.get(k);
            } else {
                write_store.set(k, "v" + std::to_string(i));
            }
        }
        auto dur = hrc::now() - start;
        printResult("Mixed R/W", N, dur);
    }

    // ── 5. TTL SET benchmark ──────────────────────────────────────────────────
    printHeader("Phase 5: SET with TTL (EX 3600)");
    {
        KVStore ttl_store(BENCH_CAP);
        auto   start = hrc::now();
        for (size_t i = 0; i < N; ++i) {
            ttl_store.set("ttlkey:" + std::to_string(i),
                          "val:" + std::to_string(i),
                          3600 /* 1 hour TTL */);
        }
        auto dur = hrc::now() - start;
        printResult("SET with TTL", N, dur);
    }

    // ── 6. LRU eviction stress ────────────────────────────────────────────────
    printHeader("Phase 6: LRU Eviction Stress (cap=1000, write 10k)");
    {
        KVStore evict_store(1000);
        auto   start = hrc::now();
        for (size_t i = 0; i < 10'000; ++i) {
            evict_store.set("ek:" + std::to_string(i), std::to_string(i));
        }
        auto dur = hrc::now() - start;
        printResult("SET (evicting)", 10'000, dur);
        auto s = evict_store.stats();
        std::cout << "  \033[90m  → " << s.evictions << " evictions, "
                  << s.current_keys << " keys remain\033[0m\n";
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "\n\033[1;36m  ==============================================\033[0m\n";
    std::cout << "  \033[1mBenchmark complete. Store stats:\033[0m\n";
    auto s = write_store.stats();
    std::cout << "  Total SETs:   " << s.sets     << "\n";
    std::cout << "  Total GETs:   " << s.hits + s.misses << "\n";
    std::cout << "  Hit ratio :   " << std::fixed << std::setprecision(1)
              << (s.hits + s.misses > 0 ? 100.0 * static_cast<double>(s.hits) / static_cast<double>(s.hits + s.misses) : 0.0)
              << "%\n";
    std::cout << "\033[1;36m  ==============================================\033[0m\n\n";

    return 0;
}
