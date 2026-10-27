#include "Json.h"

#include <locale>
#include <sstream>

namespace owlight::json {

const Value* Value::find(std::string_view key) const {
    for (const auto& m : object) if (m.key == key) return &m.value;
    return nullptr;
}

std::string_view kindName(Value::Kind k) {
    switch (k) {
        case Value::Kind::Null:   return "null";
        case Value::Kind::Bool:   return "a boolean";
        case Value::Kind::Number: return "a number";
        case Value::Kind::String: return "a string";
        case Value::Kind::Array:  return "an array";
        case Value::Kind::Object: return "an object";
    }
    return "?";
}

namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    bool document(Value& out, Error& error) {
        skipSpace();
        if (!value(out, 0)) { error = error_; return false; }
        skipSpace();
        if (pos_ != s_.size()) { fail("unexpected text after the document"); error = error_; return false; }
        return true;
    }

private:
    bool fail(const std::string& msg) {
        if (error_.message.empty()) error_ = Error{line_, msg};
        return false;
    }

    bool atEnd() const { return pos_ >= s_.size(); }
    char peek() const { return atEnd() ? '\0' : s_[pos_]; }

    void skipSpace() {
        while (!atEnd()) {
            const char c = s_[pos_];
            if (c == '\n') ++line_;
            else if (c != ' ' && c != '\t' && c != '\r') break;
            ++pos_;
        }
    }

    bool literal(std::string_view word) {
        if (s_.substr(pos_, word.size()) != word) return fail("unexpected character");
        pos_ += word.size();
        return true;
    }

    bool value(Value& out, int depth) {
        if (depth > kMaxDepth) return fail("nested too deeply");
        out.line = line_;
        switch (peek()) {
            case '{': return object(out, depth);
            case '[': return array(out, depth);
            case '"': out.kind = Value::Kind::String; return string(out.string);
            case 't': out.kind = Value::Kind::Bool; out.boolean = true;  return literal("true");
            case 'f': out.kind = Value::Kind::Bool; out.boolean = false; return literal("false");
            case 'n': out.kind = Value::Kind::Null; return literal("null");
            case '\0': return fail("unexpected end of file");
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return number(out);
                return fail(std::string("unexpected character '") + peek() + "'");
        }
    }

    bool object(Value& out, int depth) {
        out.kind = Value::Kind::Object;
        ++pos_;   // '{'
        skipSpace();
        if (peek() == '}') { ++pos_; return true; }
        for (;;) {
            skipSpace();
            if (peek() != '"') return fail("expected a key in quotes");
            Member m;
            const int keyLine = line_;
            if (!string(m.key)) return false;
            if (out.find(m.key)) { line_ = keyLine; return fail("duplicate key \"" + m.key + "\""); }
            skipSpace();
            if (peek() != ':') return fail("expected ':' after key");
            ++pos_;
            skipSpace();
            if (!value(m.value, depth + 1)) return false;
            out.object.push_back(std::move(m));
            skipSpace();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; return true; }
            return fail("expected ',' or '}'");
        }
    }

    bool array(Value& out, int depth) {
        out.kind = Value::Kind::Array;
        ++pos_;   // '['
        skipSpace();
        if (peek() == ']') { ++pos_; return true; }
        for (;;) {
            skipSpace();
            Value v;
            if (!value(v, depth + 1)) return false;
            out.array.push_back(std::move(v));
            skipSpace();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; return true; }
            return fail("expected ',' or ']'");
        }
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned& cp) {
        if (pos_ + 4 > s_.size()) return fail("bad \\u escape");
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s_[pos_++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }

    bool string(std::string& out) {
        ++pos_;   // opening quote
        for (;;) {
            if (atEnd()) return fail("unterminated string");
            const char c = s_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (atEnd()) return fail("unterminated string");
            switch (s_[pos_++]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        unsigned lo = 0;
                        if (s_.substr(pos_, 2) != "\\u") return fail("unpaired surrogate");
                        pos_ += 2;
                        if (!hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("unpaired surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("unpaired surrogate");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("bad escape in string");
            }
        }
    }

    bool digits() {
        const std::size_t start = pos_;
        while (peek() >= '0' && peek() <= '9') ++pos_;
        return pos_ > start;
    }

    bool number(Value& out) {
        out.kind = Value::Kind::Number;
        const std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        if (peek() == '0') ++pos_;
        else if (!digits()) return fail("bad number");
        if (peek() == '.') { ++pos_; if (!digits()) return fail("bad number"); }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!digits()) return fail("bad number");
        }
        // Grammar checked above; convert with the classic locale so a host that sets a
        // decimal-comma locale (as a game engine may) reads the same numbers.
        std::istringstream in(std::string(s_.substr(start, pos_ - start)));
        in.imbue(std::locale::classic());
        in >> out.number;
        if (in.fail()) return fail("number out of range");
        return true;
    }

    std::string_view s_;
    std::size_t pos_ = 0;
    int line_ = 1;
    Error error_;
};

} // namespace

bool parse(std::string_view text, Value& out, Error& error) {
    out = Value{};
    return Parser(text).document(out, error);
}

} // namespace owlight::json
