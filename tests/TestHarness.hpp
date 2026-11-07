#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ectest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

class Registry {
public:
    static Registry& instance() {
        static Registry registry;
        return registry;
    }
    void add(std::string name, std::function<void()> fn) {
        tests.push_back({std::move(name), std::move(fn)});
    }
    std::vector<TestCase> tests;
};

struct Failure {
    std::string message;
};

inline void fail(const std::string& file, int line, const std::string& expression, const std::string& detail = {}) {
    std::ostringstream ss;
    ss << file << ":" << line << " CHECK(" << expression << ") failed";
    if (!detail.empty()) ss << " — " << detail;
    throw Failure{ss.str()};
}

template <typename A, typename B>
void checkEq(const A& a, const B& b, const std::string& expression, const std::string& file, int line) {
    if (!(a == b)) {
        std::ostringstream ss;
        ss << a << " != " << b;
        fail(file, line, expression, ss.str());
    }
}

} // namespace ectest

#define EC_TEST(name) \
    static void name(); \
    static const bool ec_registered_##name = [] { \
        ::ectest::Registry::instance().add(#name, name); \
        return true; \
    }(); \
    static void name()

#define CHECK(expr) \
    do { if (!(expr)) ::ectest::fail(__FILE__, __LINE__, #expr); } while (false)

#define CHECK_MSG(expr, msg) \
    do { if (!(expr)) ::ectest::fail(__FILE__, __LINE__, #expr, (msg)); } while (false)

#define CHECK_EQ(a, b) \
    do { if (!((a) == (b))) { \
        std::ostringstream ec_ss; ec_ss << (a) << " != " << (b); \
        ::ectest::fail(__FILE__, __LINE__, #a " == " #b, ec_ss.str()); \
    } } while (false)

#define CHECK_NEAR(a, b, eps) \
    do { if (std::abs((a) - (b)) > (eps)) { \
        std::ostringstream ec_ss; ec_ss << (a) << " vs " << (b) << " eps=" << (eps); \
        ::ectest::fail(__FILE__, __LINE__, #a " ~= " #b, ec_ss.str()); \
    } } while (false)

#define CHECK_THROWS(expr) \
    do { bool ec_threw = false; try { (void)(expr); } catch (...) { ec_threw = true; } \
        if (!ec_threw) ::ectest::fail(__FILE__, __LINE__, #expr " throws"); } while (false)
