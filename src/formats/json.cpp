#include "formats/json.h"

#include <cstdio>
#include <cstdlib>

namespace chr::formats {

const Json::Object& Json::object() const {
    if (auto* o = std::get_if<Object>(&value)) return *o;
    throw JsonError("JSON: expected an object");
}
const Json::Array& Json::array() const {
    if (auto* a = std::get_if<Array>(&value)) return *a;
    throw JsonError("JSON: expected an array");
}
const std::string& Json::string() const {
    if (auto* s = std::get_if<std::string>(&value)) return *s;
    throw JsonError("JSON: expected a string");
}
double Json::number() const {
    if (auto* n = std::get_if<double>(&value)) return *n;
    throw JsonError("JSON: expected a number");
}
const Json* Json::find(std::string_view key) const {
    const auto& o = object();
    const auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
}

namespace {

class Parser {
public:
    explicit Parser(std::string_view t) : t_(t) {}

    Json document() {
        Json v = value();
        space();
        if (i_ != t_.size()) fail("text after the end");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string& why) {
        throw JsonError("JSON at byte " + std::to_string(i_) + ": " + why);
    }
    void space() {
        while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\t' || t_[i_] == '\n' || t_[i_] == '\r')) ++i_;
    }
    bool take(char c) {
        space();
        if (i_ < t_.size() && t_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }
    void expect(char c) {
        if (!take(c)) fail(std::string("expected '") + c + "'");
    }
    bool word(std::string_view w) {
        if (t_.substr(i_, w.size()) != w) return false;
        i_ += w.size();
        return true;
    }

    Json value() {
        space();
        if (i_ >= t_.size()) fail("unexpected end");
        const char c = t_[i_];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return Json{string()};
        if (word("true")) return Json{true};
        if (word("false")) return Json{false};
        if (word("null")) return Json{nullptr};
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        fail("unexpected character");
    }

    Json object() {
        expect('{');
        Json::Object o;
        if (take('}')) return Json{std::move(o)};
        do {
            space();
            if (i_ >= t_.size() || t_[i_] != '"') fail("expected a key");
            std::string key = string();
            expect(':');
            o[std::move(key)] = value();
        } while (take(','));
        expect('}');
        return Json{std::move(o)};
    }

    Json array() {
        expect('[');
        Json::Array a;
        if (take(']')) return Json{std::move(a)};
        do a.push_back(value());
        while (take(','));
        expect(']');
        return Json{std::move(a)};
    }

    std::string string() {
        ++i_;  // the opening quote
        std::string out;
        for (;;) {
            if (i_ >= t_.size()) fail("unfinished string");
            const char c = t_[i_++];
            if (c == '"') return out;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= t_.size()) fail("unfinished escape");
            const char e = t_[i_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                if (i_ + 4 > t_.size()) fail("short \\u escape");
                const std::string hex(t_.substr(i_, 4));
                char* end = nullptr;
                const long code = std::strtol(hex.c_str(), &end, 16);
                if (end != hex.c_str() + 4) fail("bad \\u escape");
                if (code > 0xff) fail("characters above \\u00ff are not used in game text");
                out += char(code);
                i_ += 4;
                break;
            }
            default: fail("unknown escape");
            }
        }
    }

    Json number() {
        const std::size_t start = i_;
        while (i_ < t_.size() && std::string_view("+-0123456789.eE").find(t_[i_]) != std::string_view::npos) ++i_;
        const std::string s(t_.substr(start, i_ - start));
        char* end = nullptr;
        const double d = std::strtod(s.c_str(), &end);
        if (end != s.c_str() + s.size()) fail("bad number");
        return Json{d};
    }

    std::string_view t_;
    std::size_t i_ = 0;
};

}  // namespace

Json parse_json(std::string_view text) { return Parser(text).document(); }

std::string json_quote(std::string_view bytes) {
    std::string out = "\"";
    for (const char ch : bytes) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\r') out += "\\r";
        else if (c == '\n') out += "\\n";
        else if (c < 0x20 || c >= 0x7f) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else out += char(c);
    }
    return out + "\"";
}

}  // namespace chr::formats
