// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_INTERNAL_JSON_H
#define USD_GEOSPATIAL_INTERNAL_JSON_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/// A small JSON reader and writer, private to the SDK implementation.
///
/// The SDK reads one document -- a composed prefix's `composition.lock.json` --
/// and writes two small ones, so it carries its own parser rather than adding a
/// third-party dependency to a package whose whole purpose is a reproducible
/// dependency set. `tools/jsonschema_lite.py` exists for the same reason on the
/// Python side. This header is not installed and is not a public contract.
namespace usd_geospatial {
namespace json {

class Value {
public:
    enum class Type { null, boolean, number, string, array, object };

    Value() = default;
    explicit Value(Type type) : _type(type) {}

    Type type() const { return _type; }
    bool is_null() const { return _type == Type::null; }
    bool is_boolean() const { return _type == Type::boolean; }
    bool is_number() const { return _type == Type::number; }
    bool is_string() const { return _type == Type::string; }
    bool is_array() const { return _type == Type::array; }
    bool is_object() const { return _type == Type::object; }

    bool boolean() const { return _boolean; }
    double number() const { return _number; }
    /// The string payload, or an empty string for any other type.
    const std::string& text() const { return _text; }
    /// The elements of an array, or an empty vector for any other type.
    const std::vector<Value>& items() const { return _items; }

    /// The member with a name, or nullptr when this is not an object or the
    /// member is absent. Callers distinguish "absent" from "present and wrong
    /// type" by checking the returned pointer and then its `type()`.
    const Value* find(const std::string& name) const;

    /// The text of a string member, or an empty string when the member is
    /// absent or is not a string.
    const std::string& member_text(const std::string& name) const;

    void set_boolean(bool value) { _type = Type::boolean; _boolean = value; }
    void set_number(double value) { _type = Type::number; _number = value; }
    void set_text(std::string value) { _type = Type::string; _text = std::move(value); }
    void append(Value value) { _type = Type::array; _items.push_back(std::move(value)); }
    void insert(std::string name, Value value) {
        _type = Type::object;
        _members.emplace_back(std::move(name), std::move(value));
    }

private:
    Type _type = Type::null;
    bool _boolean = false;
    double _number = 0.0;
    std::string _text;
    std::vector<Value> _items;
    std::vector<std::pair<std::string, Value>> _members;
};

/// Parse a complete JSON document. On failure returns false and sets `error`
/// to a message naming the byte offset; `out` is left unspecified.
bool parse(const std::string& text, Value& out, std::string& error);

/// Build JSON text in the same shape Python's `json.dumps(indent=2)` writes,
/// so a document this SDK emits and one the repository tools emit compare as
/// text. Members are written in the order they are added.
class Writer {
public:
    void begin_object();
    void end_object();
    void begin_array();
    void end_array();
    /// Name the member the next value belongs to. Objects only.
    void key(const std::string& name);
    void string(const std::string& value);
    void boolean(bool value);

    const std::string& text() const { return _text; }

private:
    void _separate();

    std::string _text;
    int _depth = 0;
    bool _pending_key = false;
    std::vector<bool> _empty;
};

/// Escape a string as a JSON string literal, including the surrounding quotes.
std::string quote(const std::string& value);

}  // namespace json
}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_INTERNAL_JSON_H
