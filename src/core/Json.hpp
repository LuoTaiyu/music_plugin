#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ec {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Json() : value_(nullptr) {}
    Json(std::nullptr_t) : value_(nullptr) {}
    Json(bool b) : value_(b) {}
    Json(int n) : value_(static_cast<double>(n)) {}
    Json(std::int64_t n) : value_(static_cast<double>(n)) {}
    Json(std::uint64_t n) : value_(static_cast<double>(n)) {}
    Json(float n) : value_(static_cast<double>(n)) {}
    Json(double n) : value_(n) {}
    Json(const char* s) : value_(std::string(s ? s : "")) {}
    Json(std::string s) : value_(std::move(s)) {}
    Json(Array a) : value_(std::move(a)) {}
    Json(Object o) : value_(std::move(o)) {}

    static Json object() { return Json(Object{}); }
    static Json array() { return Json(Array{}); }
    static Json parse(std::string_view text, std::string* error = nullptr);

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isNumber() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<Array>(value_); }
    bool isObject() const { return std::holds_alternative<Object>(value_); }

    bool asBool(bool fallback = false) const;
    double asDouble(double fallback = 0.0) const;
    std::int64_t asInt(std::int64_t fallback = 0) const;
    std::string asString(std::string fallback = {}) const;

    Json& set(const std::string& key, Json value);
    Json& push(Json value);
    const Json& operator[](const std::string& key) const;
    const Json& operator[](std::size_t index) const;
    Json& operator[](const std::string& key);
    Json& operator[](std::size_t index);
    bool contains(const std::string& key) const;
    std::size_t size() const;
    const Object& objectItems() const;
    const Array& arrayItems() const;

    std::string dump(int indent = 0) const;

private:
    Value value_;
    static Json nullValue();
    void dumpTo(std::string& out, int indent, int depth) const;
};

} // namespace ec
