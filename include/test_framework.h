#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace Color {
inline constexpr const char *RESET = "\033[0m";
inline constexpr const char *GREEN = "\033[32m";
inline constexpr const char *RED = "\033[31m";
inline constexpr const char *YELLOW = "\033[33m";
inline constexpr const char *CYAN = "\033[36m";
} // namespace Color

// Assertion helper with better error messages
#define ASSERT_TRUE(cond, msg)                                                                                                             \
    do {                                                                                                                                   \
        if (!(cond)) {                                                                                                                     \
            std::cerr << Color::RED << "  Assertion failed: " << Color::RESET << msg << std::endl;                                         \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << std::endl;                                                              \
            return false;                                                                                                                  \
        }                                                                                                                                  \
    } while (0)

#define ASSERT_EQ(actual, expected, msg)                                                                                                   \
    do {                                                                                                                                   \
        if ((actual) != (expected)) {                                                                                                      \
            std::cerr << Color::RED << "  Assertion failed: " << Color::RESET << msg << std::endl;                                         \
            std::cerr << "  Expected: " << (expected) << std::endl;                                                                        \
            std::cerr << "  Actual:   " << (actual) << std::endl;                                                                          \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << std::endl;                                                              \
            return false;                                                                                                                  \
        }                                                                                                                                  \
    } while (0)

class TestFramework {
  public:
    struct TestResult {
        std::string name;
        bool passed;
        std::string message;
    };

    explicit TestFramework(const std::string &suite_name);

    void run(const std::string &test_name, std::function<bool()> test_func);

    void printSummary();

    int exitCode() const;

  private:
    std::string suite_name_;
    int passed_;
    int failed_;
    std::vector<TestResult> results_;
};

#endif
