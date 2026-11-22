// A very small test framework, so the project needs no outside testing library.
//
//   TEST(name) { CHECK(condition); CHECK_EQ(actual, expected); }
//
// Each TEST registers itself in a list; runAllTests() runs them and reports
// how many checks failed.
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

inline void reportFailure(const char* file, int line, const std::string& message) {
    failureCount() += 1;
    std::cout << "    FAILED " << file << ":" << line << "  " << message << '\n';
}

template <typename Actual, typename Expected>
void checkEqual(const Actual& actual, const Expected& expected, const char* expression, const char* file, int line) {
    if (actual == expected) return;
    std::ostringstream message;
    message << expression << "\n      expected: " << expected << "\n      actual:   " << actual;
    reportFailure(file, line, message.str());
}

inline int runAllTests() {
    int failedTests = 0;
    for (const TestCase& test : registry()) {
        const int before = failureCount();
        try {
            test.body();
        } catch (const std::exception& error) {
            reportFailure(test.name.c_str(), 0, std::string("unexpected exception: ") + error.what());
        }
        const bool passed = failureCount() == before;
        if (!passed) failedTests += 1;
        std::cout << (passed ? "ok    " : "FAIL  ") << test.name << '\n';
    }
    std::cout << '\n' << registry().size() - static_cast<std::size_t>(failedTests) << " of " << registry().size()
              << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static const testing::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(condition) \
    do { if (!(condition)) testing::reportFailure(__FILE__, __LINE__, #condition); } while (false)

#define CHECK_EQ(actual, expected) \
    testing::checkEqual((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

// Passes only if the expression throws the given exception type.
#define CHECK_THROWS(expression, ExceptionType)                                                      \
    do {                                                                                             \
        bool threw = false;                                                                          \
        try { (void)(expression); } catch (const ExceptionType&) { threw = true; }                   \
        if (!threw) testing::reportFailure(__FILE__, __LINE__, #expression " should throw " #ExceptionType); \
    } while (false)
