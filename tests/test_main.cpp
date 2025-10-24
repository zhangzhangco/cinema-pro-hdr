#include "test_framework.h"

int main() {
    return TestFramework::TestRunner::Instance().RunAllTests();
}