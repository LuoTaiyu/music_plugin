#include "TestHarness.hpp"

#include <exception>
#include <iostream>

int main() {
    auto& registry = ectest::Registry::instance();
    int passed = 0;
    int failed = 0;
    for (const auto& test : registry.tests) {
        std::cout << "[RUN ] " << test.name << std::endl;
        try {
            test.fn();
            ++passed;
            std::cout << "[PASS] " << test.name << std::endl;
        } catch (const ectest::Failure& f) {
            ++failed;
            std::cout << "[FAIL] " << test.name << ": " << f.message << std::endl;
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << test.name << ": uncaught exception: " << e.what() << std::endl;
        } catch (...) {
            ++failed;
            std::cout << "[FAIL] " << test.name << ": unknown exception" << std::endl;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed, " << registry.tests.size() << " total"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
