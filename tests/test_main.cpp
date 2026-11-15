#include <cstdlib>
#include <exception>
#include <filesystem>

#include "test.h"

namespace chrtest {

namespace {
int failures = 0;
}

std::vector<Test>& registry() {
    static std::vector<Test> tests;
    return tests;
}

void fail(const char* file, int line, const std::string& what) {
    ++failures;
    std::printf("  FAIL %s:%d: %s\n", file, line, what.c_str());
}

void skip(const std::string& why) { std::printf("  skipped: %s\n", why.c_str()); }

std::string game_dir() {
    std::string dir;
    if (const char* env = std::getenv("CHR_GAME_DIR"); env && *env)
        dir = env;
    else
        dir = CHR_DEFAULT_GAME_DIR;
    std::error_code ec;
    return std::filesystem::is_directory(dir, ec) ? dir : std::string();
}

}  // namespace chrtest

int main() {
    for (const auto& test : chrtest::registry()) {
        std::printf("%s\n", test.name);
        try {
            test.body();
        } catch (const std::exception& e) {
            chrtest::fail(__FILE__, __LINE__, std::string("exception: ") + e.what());
        }
    }
    std::printf("%zu tests, %d failures\n", chrtest::registry().size(), chrtest::failures);
    return chrtest::failures == 0 ? 0 : 1;
}
