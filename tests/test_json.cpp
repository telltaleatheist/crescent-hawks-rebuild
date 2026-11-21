#include <string>
#include <string_view>

#include "formats/json.h"
#include "test.h"

using namespace chr;

namespace {

// The parser's message for `text`, or "" if it is accepted.
std::string error_of(std::string_view text) {
    try {
        formats::parse_json(text);
    } catch (const formats::JsonError& e) {
        return e.what();
    }
    return "";
}

bool refused(std::string_view text) { return !error_of(text).empty(); }

}  // namespace

TEST(json_accepts_what_the_data_files_use) {
    const auto j = formats::parse_json("{\"a\": [1, -2, 0, 0.5, -0.25, 1e3, 2E-2, 0e0], \"b\": \"x\\u00e9\\n\", \"c\": null, \"d\": true}");
    const auto& a = j.find("a")->array();
    CHECK(a.size() == 8);
    CHECK(a[0].number() == 1 && a[1].number() == -2 && a[2].number() == 0);
    CHECK(a[3].number() == 0.5 && a[4].number() == -0.25 && a[5].number() == 1000 && a[6].number() == 0.02);
    CHECK(j.find("b")->string() == "x\xe9\n");
    CHECK(error_of("[1,]") != "");  // a trailing comma
}

TEST(json_refuses_a_repeated_key) {
    CHECK(refused(R"({"a": 1, "a": 2})"));
    CHECK(error_of(R"({"a": 1, "a": 2})").find("duplicate key") != std::string::npos);
    CHECK(!refused(R"({"a": {"a": 1}})"));  // the same key at another level is fine
}

TEST(json_refuses_raw_control_characters_in_strings) {
    CHECK(refused("[\"a\tb\"]"));
    CHECK(refused("[\"a\nb\"]"));
    CHECK(refused(std::string_view("[\"a\0b\"]", 7)));
    CHECK(refused("[\"\x1f\"]"));
    CHECK(!refused("[\"a\\tb\\u0001\"]"));  // escaped, they are fine
}

TEST(json_u_escape_needs_four_hex_digits) {
    CHECK(refused("[\"\\u 0ff\"]"));
    CHECK(refused("[\"\\u-001\"]"));
    CHECK(refused("[\"\\u+0ff\"]"));
    CHECK(refused("[\"\\u00g1\"]"));
    CHECK(refused("[\"\\u00f\"]"));
    CHECK(formats::parse_json("[\"\\u00Ff\"]").array()[0].string() == "\xff");
}

TEST(json_numbers_follow_the_grammar) {
    CHECK(refused("[01]"));
    CHECK(refused("[-01]"));
    CHECK(refused("[00]"));
    CHECK(refused("[+1]"));
    CHECK(refused("[1.]"));
    CHECK(refused("[.5]"));
    CHECK(refused("[1e]"));
    CHECK(refused("[1e+]"));
    CHECK(refused("[-]"));
    CHECK(refused("[1-2]"));
    CHECK(error_of("[01]").find("leading zero") != std::string::npos);
    // Read without the locale: the decimal point is always '.'.
    CHECK(formats::parse_json("[1.5]").array()[0].number() == 1.5);
}

TEST(json_refuses_deep_nesting) {
    const std::string deep(100000, '[');
    CHECK(error_of(deep).find("nested deeper") != std::string::npos);
    std::string ok(256, '[');
    ok += std::string(256, ']');
    CHECK(!refused(ok));
    std::string over(257, '[');
    over += std::string(257, ']');
    CHECK(refused(over));
    std::string objects;
    for (int i = 0; i < 300; ++i) objects += "{\"a\":";
    CHECK(refused(objects));
}

TEST(json_errors_give_line_and_column) {
    CHECK(error_of("{\n  \"a\": 1,\n  \"a\": 2\n}").find("line 3, column 3") != std::string::npos);
    CHECK(error_of("[1, 2,\n 03]").find("line 2, column 2") != std::string::npos);
    CHECK(error_of("[\"ab\tc\"]").find("line 1, column 5") != std::string::npos);
}
