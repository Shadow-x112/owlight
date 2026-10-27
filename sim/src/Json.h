#pragma once
// A small strict JSON reader for the catalog data files. Internal to the sim; not a public
// header. Keeps object members in file order and records the line each value starts on, so
// loaders can report "types.json:42: ..." errors that point at the hand-edited line.

#include <string>
#include <string_view>
#include <vector>

namespace owlight::json {

struct Member;

struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    int line = 0;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<Member> object;   // file order; duplicate keys are rejected by parse()

    const Value* find(std::string_view key) const;
};

struct Member {
    std::string key;
    Value value;
};

std::string_view kindName(Value::Kind k);

struct Error {
    int line = 0;
    std::string message;
};

// Parse a complete JSON document (RFC 8259, no extensions). On failure returns false and fills
// `error` with the line and what went wrong.
bool parse(std::string_view text, Value& out, Error& error);

} // namespace owlight::json
