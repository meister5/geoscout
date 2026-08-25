// Minimal assertion harness. Deliberately not Unity or doctest: these tests must
// build both under `pio test -e native` (test_framework = custom, so each test
// provides its own main) and under a plain `make test` with nothing installed
// but a compiler.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace check {
inline int failures = 0;
inline int checks = 0;

inline void report(bool ok, const char* file, int line, const char* expr) {
    ++checks;
    if (ok) return;
    ++failures;
    std::printf("  FAIL %s:%d  %s\n", file, line, expr);
}

inline int finish(const char* suite) {
    if (failures == 0) {
        std::printf("PASS %-22s %3d checks\n", suite, checks);
        return 0;
    }
    std::printf("FAIL %-22s %d of %d checks failed\n", suite, failures, checks);
    return 1;
}
}  // namespace check

#define CHECK_TRUE(expr) ::check::report((expr), __FILE__, __LINE__, #expr)
#define CHECK_FALSE(expr) ::check::report(!(expr), __FILE__, __LINE__, "!(" #expr ")")

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        auto _a = (a);                                                        \
        auto _b = (b);                                                        \
        bool _ok = (_a == _b);                                                \
        ::check::report(_ok, __FILE__, __LINE__, #a " == " #b);               \
        if (!_ok) std::printf("       got %lld, want %lld\n",                 \
                              static_cast<long long>(_a),                     \
                              static_cast<long long>(_b));                    \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                 \
    do {                                                                      \
        double _a = static_cast<double>(a);                                   \
        double _b = static_cast<double>(b);                                   \
        bool _ok = std::fabs(_a - _b) <= (tol);                               \
        ::check::report(_ok, __FILE__, __LINE__, #a " ~= " #b);               \
        if (!_ok) std::printf("       got %.6f, want %.6f (tol %.6f)\n",      \
                              _a, _b, static_cast<double>(tol));              \
    } while (0)

#define CHECK_STREQ(a, b)                                                     \
    do {                                                                      \
        std::string _a(a);                                                    \
        std::string _b(b);                                                    \
        bool _ok = (_a == _b);                                                \
        ::check::report(_ok, __FILE__, __LINE__, #a " == " #b);               \
        if (!_ok) std::printf("       got [%s]\n       want [%s]\n",          \
                              _a.c_str(), _b.c_str());                        \
    } while (0)

#define CHECK_CONTAINS(haystack, needle)                                      \
    do {                                                                      \
        std::string _h(haystack);                                             \
        std::string _n(needle);                                               \
        bool _ok = _h.find(_n) != std::string::npos;                          \
        ::check::report(_ok, __FILE__, __LINE__, "contains " #needle);        \
        if (!_ok) std::printf("       [%s]\n       lacks [%s]\n",             \
                              _h.c_str(), _n.c_str());                        \
    } while (0)
