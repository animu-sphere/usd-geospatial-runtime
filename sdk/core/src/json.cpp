// SPDX-License-Identifier: Apache-2.0
#include "json.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace usd_geospatial {
namespace json {
namespace {

/// Nesting limit. The documents this parser reads are a handful of levels
/// deep; the limit exists so a malformed or hostile file exhausts a counter
/// instead of the call stack.
constexpr int kMaxDepth = 64;

const std::string kEmptyText;

void append_utf8(std::string& out, std::uint32_t code_point) {
    if (code_point < 0x80) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

class Parser {
public:
    Parser(const std::string& text, std::string& error, MemberFilter keep)
        : _text(text), _error(error), _keep(keep) {}

    bool run(Value& out) {
        skip_space();
        if (!parse_value(out, 0)) {
            return false;
        }
        skip_space();
        if (_at != _text.size()) {
            return fail("trailing content after the document");
        }
        return true;
    }

private:
    bool fail(const char* what) {
        _error = std::string(what) + " at byte " + std::to_string(_at);
        return false;
    }

    bool at_end() const { return _at >= _text.size(); }
    char peek() const { return _text[_at]; }

    void skip_space() {
        while (!at_end()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++_at;
            } else {
                break;
            }
        }
    }

    bool literal(const char* word) {
        const std::size_t length = std::char_traits<char>::length(word);
        if (_text.compare(_at, length, word) != 0) {
            return fail("unrecognized literal");
        }
        _at += length;
        return true;
    }

    bool parse_string(std::string& out) {
        if (at_end() || peek() != '"') {
            return fail("expected a string");
        }
        ++_at;
        out.clear();
        while (true) {
            if (at_end()) {
                return fail("unterminated string");
            }
            const char c = _text[_at++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("unescaped control character in a string");
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (at_end()) {
                return fail("unterminated escape");
            }
            const char escape = _text[_at++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t code_point = 0;
                    if (!parse_hex4(code_point)) {
                        return false;
                    }
                    // A surrogate pair arrives as two escapes; combine them so
                    // the decoded text is well-formed UTF-8 rather than two
                    // isolated surrogates.
                    if (code_point >= 0xD800 && code_point <= 0xDBFF &&
                        _text.compare(_at, 2, "\\u") == 0) {
                        const std::size_t mark = _at;
                        _at += 2;
                        std::uint32_t low = 0;
                        if (!parse_hex4(low)) {
                            return false;
                        }
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            _at = mark;
                        }
                    }
                    append_utf8(out, code_point);
                    break;
                }
                default:
                    return fail("unrecognized escape");
            }
        }
    }

    bool parse_hex4(std::uint32_t& out) {
        if (_at + 4 > _text.size()) {
            return fail("truncated unicode escape");
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = _text[_at++];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return fail("non-hexadecimal digit in a unicode escape");
            }
        }
        return true;
    }

    /// Scan a number per the JSON grammar: `-? (0 | [1-9][0-9]*) (. [0-9]+)?
    /// ([eE] [+-]? [0-9]+)?`. The grammar is applied rather than "advance over
    /// anything numeric-looking" so a malformed value such as `1-2e+3` is
    /// rejected here instead of surviving as a number.
    bool scan_number() {
        const auto digits = [this]() {
            const std::size_t start = _at;
            while (!at_end() && peek() >= '0' && peek() <= '9') {
                ++_at;
            }
            return _at > start;
        };
        if (!at_end() && peek() == '-') {
            ++_at;
        }
        if (at_end()) {
            return fail("expected a value");
        }
        if (peek() == '0') {
            ++_at;
        } else if (!digits()) {
            return fail("expected a value");
        }
        if (!at_end() && peek() == '.') {
            ++_at;
            if (!digits()) {
                return fail("malformed number");
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            ++_at;
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                ++_at;
            }
            if (!digits()) {
                return fail("malformed number");
            }
        }
        return true;
    }

    bool parse_number(Value& out) {
        const std::size_t start = _at;
        if (!scan_number()) {
            return false;
        }
        // JSON always spells a decimal point as '.', so the conversion must not
        // follow the host locale. A process that has called setlocale would
        // otherwise reject every fractional number on a comma-decimal machine.
        // std::from_chars would be the direct tool, but its floating-point half
        // is still missing from standard libraries this SDK expects to target.
        std::istringstream reader(_text.substr(start, _at - start));
        reader.imbue(std::locale::classic());
        double value = 0.0;
        reader >> value;
        if (reader.fail()) {
            return fail("malformed number");
        }
        out.set_number(value);
        return true;
    }

    bool parse_value(Value& out, int depth) {
        if (depth > kMaxDepth) {
            return fail("document nesting exceeds the supported depth");
        }
        if (at_end()) {
            return fail("expected a value");
        }
        switch (peek()) {
            case '{': return parse_object(out, depth);
            case '[': return parse_array(out, depth);
            case '"': {
                std::string text;
                if (!parse_string(text)) {
                    return false;
                }
                out.set_text(std::move(text));
                return true;
            }
            case 't':
                if (!literal("true")) {
                    return false;
                }
                out.set_boolean(true);
                return true;
            case 'f':
                if (!literal("false")) {
                    return false;
                }
                out.set_boolean(false);
                return true;
            case 'n':
                if (!literal("null")) {
                    return false;
                }
                out = Value(Value::Type::null);
                return true;
            default:
                return parse_number(out);
        }
    }

    /// Advance past one value without building anything for it.
    ///
    /// This is what makes `keep` worth having: a skipped value costs a scan
    /// and no allocation, so the 5 MB of file inventory in a composition lock
    /// never becomes 100,000 nodes on the way to reading twenty fields.
    bool skip_value(int depth) {
        if (depth > kMaxDepth) {
            return fail("document nesting exceeds the supported depth");
        }
        if (at_end()) {
            return fail("expected a value");
        }
        switch (peek()) {
            case '"':
                return skip_string();
            case 't':
                return literal("true");
            case 'f':
                return literal("false");
            case 'n':
                return literal("null");
            case '[':
            case '{': {
                const bool object = peek() == '{';
                const char close = object ? '}' : ']';
                ++_at;
                skip_space();
                if (!at_end() && peek() == close) {
                    ++_at;
                    return true;
                }
                while (true) {
                    skip_space();
                    if (object) {
                        if (!skip_string()) {
                            return false;
                        }
                        skip_space();
                        if (at_end() || peek() != ':') {
                            return fail("expected a colon after an object member name");
                        }
                        ++_at;
                        skip_space();
                    }
                    if (!skip_value(depth + 1)) {
                        return false;
                    }
                    skip_space();
                    if (at_end()) {
                        return fail(object ? "unterminated object" : "unterminated array");
                    }
                    if (peek() == ',') {
                        ++_at;
                        continue;
                    }
                    if (peek() == close) {
                        ++_at;
                        return true;
                    }
                    return fail(object ? "expected a comma or the end of an object"
                                       : "expected a comma or the end of an array");
                }
            }
            default:
                return scan_number();
        }
    }

    /// Advance past a string without decoding it. Escapes are not interpreted,
    /// only stepped over, so a `\"` does not end the string.
    bool skip_string() {
        if (at_end() || peek() != '"') {
            return fail("expected a string");
        }
        ++_at;
        while (true) {
            if (at_end()) {
                return fail("unterminated string");
            }
            const char c = _text[_at++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("unescaped control character in a string");
            }
            if (c == '\\') {
                if (at_end()) {
                    return fail("unterminated escape");
                }
                ++_at;
            }
        }
    }

    bool parse_array(Value& out, int depth) {
        ++_at;  // consume '['
        out = Value(Value::Type::array);
        skip_space();
        if (!at_end() && peek() == ']') {
            ++_at;
            return true;
        }
        while (true) {
            Value element;
            skip_space();
            if (!parse_value(element, depth + 1)) {
                return false;
            }
            out.append(std::move(element));
            skip_space();
            if (at_end()) {
                return fail("unterminated array");
            }
            if (peek() == ',') {
                ++_at;
                continue;
            }
            if (peek() == ']') {
                ++_at;
                return true;
            }
            return fail("expected a comma or the end of an array");
        }
    }

    bool parse_object(Value& out, int depth) {
        ++_at;  // consume '{'
        out = Value(Value::Type::object);
        skip_space();
        if (!at_end() && peek() == '}') {
            ++_at;
            return true;
        }
        while (true) {
            skip_space();
            std::string name;
            if (!parse_string(name)) {
                return false;
            }
            skip_space();
            if (at_end() || peek() != ':') {
                return fail("expected a colon after an object member name");
            }
            ++_at;
            skip_space();
            if (_keep != nullptr && !_keep(_path, name)) {
                if (!skip_value(depth + 1)) {
                    return false;
                }
            } else {
                Value value;
                _path.push_back(name);
                const bool ok = parse_value(value, depth + 1);
                _path.pop_back();
                if (!ok) {
                    return false;
                }
                out.insert(std::move(name), std::move(value));
            }
            skip_space();
            if (at_end()) {
                return fail("unterminated object");
            }
            if (peek() == ',') {
                ++_at;
                continue;
            }
            if (peek() == '}') {
                ++_at;
                return true;
            }
            return fail("expected a comma or the end of an object");
        }
    }

    const std::string& _text;
    std::string& _error;
    MemberFilter _keep;
    std::vector<std::string> _path;
    std::size_t _at = 0;
};

}  // namespace

const Value* Value::find(const std::string& name) const {
    if (_type != Type::object) {
        return nullptr;
    }
    for (const auto& member : _members) {
        if (member.first == name) {
            return &member.second;
        }
    }
    return nullptr;
}

const std::string& Value::member_text(const std::string& name) const {
    const Value* member = find(name);
    if (member == nullptr || !member->is_string()) {
        return kEmptyText;
    }
    return member->text();
}

bool parse(const std::string& text, Value& out, std::string& error, MemberFilter keep) {
    Parser parser(text, error, keep);
    return parser.run(out);
}

std::string quote(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escape[7];
                    std::snprintf(escape, sizeof(escape), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += escape;
                } else {
                    // Bytes at or above 0x20 pass through, so valid UTF-8 in
                    // stays valid UTF-8 out. What this SDK writes is digests
                    // and identifiers, which are ASCII.
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
    return out;
}

void Writer::_separate() {
    // A value that follows a key belongs on the same line as that key; every
    // other value opens a new indented line and needs the preceding comma.
    if (_pending_key) {
        _pending_key = false;
        return;
    }
    if (!_empty.empty()) {
        if (!_empty.back()) {
            _text.push_back(',');
        }
        _empty.back() = false;
        _text.push_back('\n');
        _text.append(static_cast<std::size_t>(_depth) * 2, ' ');
    }
}

void Writer::begin_object() {
    _separate();
    _text.push_back('{');
    ++_depth;
    _empty.push_back(true);
}

void Writer::end_object() {
    const bool empty = _empty.back();
    _empty.pop_back();
    --_depth;
    if (!empty) {
        _text.push_back('\n');
        _text.append(static_cast<std::size_t>(_depth) * 2, ' ');
    }
    _text.push_back('}');
}

void Writer::begin_array() {
    _separate();
    _text.push_back('[');
    ++_depth;
    _empty.push_back(true);
}

void Writer::end_array() {
    const bool empty = _empty.back();
    _empty.pop_back();
    --_depth;
    if (!empty) {
        _text.push_back('\n');
        _text.append(static_cast<std::size_t>(_depth) * 2, ' ');
    }
    _text.push_back(']');
}

void Writer::key(const std::string& name) {
    _separate();
    _text += quote(name);
    _text += ": ";
    _pending_key = true;
}

void Writer::string(const std::string& value) {
    _separate();
    _text += quote(value);
}

void Writer::boolean(bool value) {
    _separate();
    _text += value ? "true" : "false";
}

}  // namespace json
}  // namespace usd_geospatial
