#include "core/Json.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace ec {
namespace {

const Json& nullRef() {
    static const Json null;
    return null;
}

std::string encodeUtf8(std::uint32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

class Parser {
public:
    explicit Parser(std::string_view in) : s_(in) {}

    Json parse(std::string& error) {
        skipWs();
        Json result = parseValue();
        if (error_.empty()) {
            skipWs();
            if (pos_ != s_.size()) fail("trailing characters");
        }
        if (!error_.empty()) error = error_;
        return result;
    }

private:
    std::string_view s_;
    std::size_t pos_ = 0;
    std::string error_;

    void fail(const std::string& message) {
        if (error_.empty()) {
            error_ = message + " at byte " + std::to_string(pos_);
        }
    }

    void skipWs() {
        while (pos_ < s_.size()) {
            const char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool consume(char c) {
        if (pos_ < s_.size() && s_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char c) {
        if (!consume(c)) fail(std::string("expected '") + c + "'");
    }

    Json parseValue() {
        if (!error_.empty()) return {};
        skipWs();
        if (pos_ >= s_.size()) {
            fail("unexpected end of input");
            return {};
        }
        const char c = s_[pos_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Json(parseString());
        if (c == 't') { parseLiteral("true"); return Json(true); }
        if (c == 'f') { parseLiteral("false"); return Json(false); }
        if (c == 'n') { parseLiteral("null"); return Json(nullptr); }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail("invalid value");
        return {};
    }

    void parseLiteral(std::string_view literal) {
        if (s_.substr(pos_, literal.size()) != literal) {
            fail("invalid literal");
            return;
        }
        pos_ += literal.size();
    }

    Json parseObject() {
        expect('{');
        Json result = Json::object();
        skipWs();
        if (consume('}')) return result;
        while (error_.empty()) {
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != '"') {
                fail("expected object key");
                return result;
            }
            std::string key = parseString();
            skipWs();
            expect(':');
            Json value = parseValue();
            if (!error_.empty()) return result;
            result.set(key, std::move(value));
            skipWs();
            if (consume('}')) break;
            expect(',');
        }
        return result;
    }

    Json parseArray() {
        expect('[');
        Json result = Json::array();
        skipWs();
        if (consume(']')) return result;
        while (error_.empty()) {
            Json value = parseValue();
            if (!error_.empty()) return result;
            result.push(std::move(value));
            skipWs();
            if (consume(']')) break;
            expect(',');
        }
        return result;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < s_.size()) {
            const unsigned char c = static_cast<unsigned char>(s_[pos_++]);
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= s_.size()) { fail("bad escape"); return out; }
                const char e = s_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > s_.size()) { fail("short unicode escape"); return out; }
                        std::uint32_t cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = s_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<std::uint32_t>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<std::uint32_t>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<std::uint32_t>(h - 'A' + 10);
                            else { fail("bad unicode escape"); return out; }
                        }
                        out += encodeUtf8(cp);
                        break;
                    }
                    default: fail("bad escape"); return out;
                }
            } else if (c < 0x20) {
                fail("control character in string");
                return out;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        fail("unterminated string");
        return out;
    }

    Json parseNumber() {
        const std::size_t start = pos_;
        if (consume('-')) {}
        if (pos_ >= s_.size()) { fail("bad number"); return {}; }
        if (s_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        double value = 0.0;
        const auto* begin = s_.data() + start;
        const auto* end = s_.data() + pos_;
        const auto r = std::from_chars(begin, end, value);
        if (r.ec != std::errc{} || r.ptr != end) {
            fail("invalid number");
            return {};
        }
        return Json(value);
    }
};

} // namespace

Json Json::parse(std::string_view text, std::string* error) {
    Parser p(text);
    std::string err;
    Json value = p.parse(err);
    if (error) *error = err;
    return value;
}

bool Json::asBool(bool fallback) const {
    if (const auto* b = std::get_if<bool>(&value_)) return *b;
    if (const auto* n = std::get_if<double>(&value_)) return *n != 0.0;
    return fallback;
}

double Json::asDouble(double fallback) const {
    if (const auto* n = std::get_if<double>(&value_)) return *n;
    return fallback;
}

std::int64_t Json::asInt(std::int64_t fallback) const {
    if (const auto* n = std::get_if<double>(&value_)) {
        if (!std::isfinite(*n)) return fallback;
        return static_cast<std::int64_t>(*n);
    }
    return fallback;
}

std::string Json::asString(std::string fallback) const {
    if (const auto* s = std::get_if<std::string>(&value_)) return *s;
    return fallback;
}

Json& Json::set(const std::string& key, Json value) {
    if (!isObject()) value_ = Object{};
    auto& obj = std::get<Object>(value_);
    for (auto& [k, v] : obj) {
        if (k == key) { v = std::move(value); return *this; }
    }
    obj.emplace_back(key, std::move(value));
    return *this;
}

Json& Json::push(Json value) {
    if (!isArray()) value_ = Array{};
    std::get<Array>(value_).push_back(std::move(value));
    return *this;
}

const Json& Json::operator[](const std::string& key) const {
    if (const auto* obj = std::get_if<Object>(&value_)) {
        for (const auto& [k, v] : *obj) if (k == key) return v;
    }
    return nullRef();
}

const Json& Json::operator[](std::size_t index) const {
    if (const auto* arr = std::get_if<Array>(&value_)) {
        if (index < arr->size()) return (*arr)[index];
    }
    return nullRef();
}

Json& Json::operator[](const std::string& key) {
    if (!isObject()) value_ = Object{};
    auto& obj = std::get<Object>(value_);
    for (auto& [k, v] : obj) if (k == key) return v;
    obj.emplace_back(key, Json(nullptr));
    return obj.back().second;
}

Json& Json::operator[](std::size_t index) {
    if (!isArray()) value_ = Array{};
    auto& arr = std::get<Array>(value_);
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

bool Json::contains(const std::string& key) const {
    if (const auto* obj = std::get_if<Object>(&value_)) {
        for (const auto& [k, v] : *obj) if (k == key) return true;
    }
    return false;
}

std::size_t Json::size() const {
    if (const auto* arr = std::get_if<Array>(&value_)) return arr->size();
    if (const auto* obj = std::get_if<Object>(&value_)) return obj->size();
    return 0;
}

const Json::Object& Json::objectItems() const {
    static const Object empty;
    if (const auto* obj = std::get_if<Object>(&value_)) return *obj;
    return empty;
}

const Json::Array& Json::arrayItems() const {
    static const Array empty;
    if (const auto* arr = std::get_if<Array>(&value_)) return *arr;
    return empty;
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const auto newline = [&](int d) {
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<std::size_t>(indent * d), ' ');
        }
    };
    if (std::holds_alternative<std::nullptr_t>(value_)) {
        out += "null";
    } else if (const auto* b = std::get_if<bool>(&value_)) {
        out += *b ? "true" : "false";
    } else if (const auto* n = std::get_if<double>(&value_)) {
        if (!std::isfinite(*n)) { out += "0"; return; }
        if (*n == 0.0) { out += "0"; return; }
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << std::setprecision(10) << *n;
        out += ss.str();
    } else if (const auto* s = std::get_if<std::string>(&value_)) {
        out.push_back('"');
        for (const unsigned char c : *s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        static const char* hex = "0123456789abcdef";
                        out += "\\u00";
                        out.push_back(hex[(c >> 4) & 0xF]);
                        out.push_back(hex[c & 0xF]);
                    } else {
                        out.push_back(static_cast<char>(c));
                    }
            }
        }
        out.push_back('"');
    } else if (const auto* arr = std::get_if<Array>(&value_)) {
        out.push_back('[');
        for (std::size_t i = 0; i < arr->size(); ++i) {
            if (i) out.push_back(',');
            newline(depth + 1);
            (*arr)[i].dumpTo(out, indent, depth + 1);
        }
        if (!arr->empty()) newline(depth);
        out.push_back(']');
    } else if (const auto* obj = std::get_if<Object>(&value_)) {
        out.push_back('{');
        for (std::size_t i = 0; i < obj->size(); ++i) {
            if (i) out.push_back(',');
            newline(depth + 1);
            const auto& [k, v] = (*obj)[i];
            out.push_back('"');
            for (const unsigned char c : k) {
                if (c == '"' || c == '\\') out.push_back('\\');
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
            }
            out += indent > 0 ? "\": " : "\":";
            v.dumpTo(out, indent, depth + 1);
        }
        if (!obj->empty()) newline(depth);
        out.push_back('}');
    }
}

Json Json::nullValue() { return Json(nullptr); }

} // namespace ec
