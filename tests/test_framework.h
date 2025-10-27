#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>

// Simple testing framework for Cinema Pro HDR
namespace TestFramework {

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class TestRunner {
public:
    static TestRunner& Instance() {
        static TestRunner instance;
        return instance;
    }
    
    void AddTest(const std::string& name, std::function<bool()> test_func) {
        tests_.push_back({name, test_func});
    }
    
    int RunAllTests() {
        int passed = 0;
        int total = tests_.size();
        
        std::cout << "Running " << total << " tests...\n\n";
        
        for (const auto& test : tests_) {
            std::cout << "Running: " << test.name << " ... ";
            
            try {
                bool result = test.func();
                if (result) {
                    std::cout << "PASSED\n";
                    passed++;
                } else {
                    std::cout << "FAILED\n";
                }
            } catch (const std::exception& e) {
                std::cout << "FAILED (exception: " << e.what() << ")\n";
            } catch (...) {
                std::cout << "FAILED (unknown exception)\n";
            }
        }
        
        std::cout << "\nResults: " << passed << "/" << total << " tests passed\n";
        
        if (passed == total) {
            std::cout << "All tests PASSED!\n";
            return 0;
        } else {
            std::cout << (total - passed) << " tests FAILED!\n";
            return 1;
        }
    }
    
private:
    struct Test {
        std::string name;
        std::function<bool()> func;
    };
    
    std::vector<Test> tests_;
};

// Test registration macro
#define TEST(name) \
    bool test_##name(); \
    static bool register_##name = []() { \
        TestFramework::TestRunner::Instance().AddTest(#name, test_##name); \
        return true; \
    }(); \
    bool test_##name()

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cout << "ASSERTION FAILED: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cout << "ASSERTION FAILED: !(" << #condition << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cout << "ASSERTION FAILED: " << #expected << " == " << #actual \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        if (std::abs((expected) - (actual)) > (tolerance)) { \
            std::cout << "ASSERTION FAILED: " << #expected << " ≈ " << #actual \
                      << " (expected: " << (expected) << ", actual: " << (actual) \
                      << ", tolerance: " << (tolerance) << ")" \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_GT(value, threshold) \
    do { \
        if ((value) <= (threshold)) { \
            std::cout << "ASSERTION FAILED: " << #value << " > " << #threshold \
                      << " (value: " << (value) << ", threshold: " << (threshold) << ")" \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_LT(value, threshold) \
    do { \
        if ((value) >= (threshold)) { \
            std::cout << "ASSERTION FAILED: " << #value << " < " << #threshold \
                      << " (value: " << (value) << ", threshold: " << (threshold) << ")" \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_GE(value, threshold) \
    do { \
        if ((value) < (threshold)) { \
            std::cout << "ASSERTION FAILED: " << #value << " >= " << #threshold \
                      << " (value: " << (value) << ", threshold: " << (threshold) << ")" \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_LE(value, threshold) \
    do { \
        if ((value) > (threshold)) { \
            std::cout << "ASSERTION FAILED: " << #value << " <= " << #threshold \
                      << " (value: " << (value) << ", threshold: " << (threshold) << ")" \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

} // namespace TestFramework