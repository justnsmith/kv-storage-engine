#include "test_framework.h"

TestFramework::TestFramework(const std::string &suite_name) : suite_name_(suite_name), passed_(0), failed_(0) {
}

void TestFramework::run(const std::string &test_name, std::function<bool()> test_func) {
    std::cout << Color::CYAN << "[ RUN    ] " << Color::RESET << test_name << std::endl;

    try {
        bool result = test_func();

        if (result) {
            std::cout << Color::GREEN << "[ PASS   ] " << Color::RESET << test_name << std::endl;
            ++passed_;
            results_.push_back({test_name, true, ""});
        } else {
            std::cout << Color::RED << "[ FAIL   ] " << Color::RESET << test_name << std::endl;
            ++failed_;
            results_.push_back({test_name, false, ""});
        }
    } catch (const std::exception &e) {
        std::cout << Color::RED << "[ ERROR  ] " << Color::RESET << test_name << ": " << e.what() << std::endl;
        ++failed_;
        results_.push_back({test_name, false, e.what()});
    }

    std::cout << std::endl;
}

void TestFramework::printSummary() {
    std::cout << "========================================" << std::endl;
    std::cout << "Test Suite: " << suite_name_ << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << Color::GREEN << "Passed: " << passed_ << Color::RESET << std::endl;

    if (failed_ > 0) {
        std::cout << Color::RED << "Failed: " << failed_ << Color::RESET << std::endl;
    } else {
        std::cout << "Failed: " << failed_ << std::endl;
    }

    std::cout << "Total:  " << (passed_ + failed_) << std::endl;
    std::cout << "========================================" << std::endl;
}

int TestFramework::exitCode() const {
    return failed_ == 0 ? 0 : 1;
}
