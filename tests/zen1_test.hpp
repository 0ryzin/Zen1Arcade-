// SPDX-License-Identifier: GPL-3.0-or-later
// Tiny self-contained test harness (no external dependency).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace zen1test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}

struct Register {
    Register(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

inline int runAll() {
    int failedCases = 0;
    for (auto& c : registry()) {
        const int before = failures();
        c.fn();
        const bool ok = failures() == before;
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failedCases;
    }
    std::printf("\n%zu tests, %d failed\n", registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}

} // namespace zen1test

#define ZEN1_CAT2(a, b) a##b
#define ZEN1_CAT(a, b) ZEN1_CAT2(a, b)
#define TEST(name)                                                                   \
    static void ZEN1_CAT(test_fn_, __LINE__)();                                      \
    static zen1test::Register ZEN1_CAT(test_reg_, __LINE__)(name, ZEN1_CAT(test_fn_, __LINE__)); \
    static void ZEN1_CAT(test_fn_, __LINE__)()

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::printf("    %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr);  \
            ++zen1test::failures();                                                   \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                \
    do {                                                                              \
        const auto va_ = (a); /* copies: operands may be temporaries */               \
        const auto vb_ = (b);                                                         \
        if (!(va_ == vb_)) {                                                          \
            std::printf("    %s:%d: CHECK_EQ failed: %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++zen1test::failures();                                                   \
        }                                                                             \
    } while (0)
