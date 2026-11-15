// A very small test harness: CHECK records a failure and carries on.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace chrtest {

struct Test {
    const char* name;
    std::function<void()> body;
};

std::vector<Test>& registry();
void fail(const char* file, int line, const std::string& what);
// The game folder, or empty if it isn't there: tests that need it skip.
std::string game_dir();
void skip(const std::string& why);

struct Register {
    Register(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

}  // namespace chrtest

#define CHR_CAT2(a, b) a##b
#define CHR_CAT(a, b) CHR_CAT2(a, b)
#define TEST(name)                                                             \
    static void name();                                                        \
    static chrtest::Register CHR_CAT(reg_, name)(#name, name);                 \
    static void name()
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) chrtest::fail(__FILE__, __LINE__, #cond);                 \
    } while (0)
