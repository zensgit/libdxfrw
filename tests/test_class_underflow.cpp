/******************************************************************************
**  libDXFrw - Class Count Underflow Tests                                   **
**                                                                           **
**  Copyright (C) 2025 libdxfrw contributors                                **
**                                                                           **
**  This library is free software, licensed under the terms of the GNU       **
**  General Public License as published by the Free Software Foundation,     **
**  either version 2 of the License, or (at your option) any later version.  **
**  You should have received a copy of the GNU General Public License        **
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.    **
******************************************************************************/

/**
 * Test cases for the maxClassNum underflow fix.
 *
 * Background:
 * In DWG format, class numbers 0-499 are reserved for built-in classes.
 * Custom classes start at number 500. The original code calculated:
 *   endDataPos = maxClassNum - 499
 *
 * This caused an integer underflow when maxClassNum <= 499 (e.g., 236 in
 * AC1032 files from DWG TrueView), resulting in:
 *   endDataPos = 236 - 499 = 4294967033 (unsigned wraparound)
 *
 * This caused massive memory allocation (80GB+) and infinite loop.
 *
 * The fix:
 *   endDataPos = (maxClassNum > 499) ? (maxClassNum - 499) : 0
 */

#include <iostream>
#include <cstdint>
#include <limits>
#include <cassert>

// Type definitions matching libdxfrw
typedef uint32_t duint32;

/**
 * Original calculation (BUGGY - causes underflow)
 */
duint32 calculateEndDataPos_Original(duint32 maxClassNum) {
    return maxClassNum - 499;
}

/**
 * Fixed calculation (safe)
 */
duint32 calculateEndDataPos_Fixed(duint32 maxClassNum) {
    return (maxClassNum > 499) ? (maxClassNum - 499) : 0;
}

/**
 * Test Case 1: Normal case - maxClassNum > 499
 * Classes 500-516 means 17 custom classes (516 - 499 = 17)
 */
bool testNormalCase() {
    std::cout << "\n=== Test: Normal Case (maxClassNum > 499) ===" << std::endl;

    // Test case from working AC1027 file: maxClassNum = 516
    duint32 maxClassNum = 516;
    duint32 expected = 17;

    duint32 result = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  Expected endDataPos = " << expected << std::endl;
    std::cout << "  Actual endDataPos = " << result << std::endl;

    if (result != expected) {
        std::cout << "  FAIL: Result does not match expected" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 2: Bug case - maxClassNum < 499
 * This is the case that caused the memory explosion bug
 * AC1032 files from DWG TrueView have maxClassNum = 236
 */
bool testBugCase_LessThan499() {
    std::cout << "\n=== Test: Bug Case (maxClassNum < 499) ===" << std::endl;

    // Test case from problematic AC1032 file: maxClassNum = 236
    duint32 maxClassNum = 236;
    duint32 expected = 0;  // Should be 0, not 4294967033

    // First, demonstrate the bug
    duint32 buggyResult = calculateEndDataPos_Original(maxClassNum);
    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  BUGGY calculation: " << buggyResult << " (would loop billions of times!)" << std::endl;

    // Now test the fix
    duint32 fixedResult = calculateEndDataPos_Fixed(maxClassNum);
    std::cout << "  FIXED calculation: " << fixedResult << std::endl;
    std::cout << "  Expected: " << expected << std::endl;

    if (fixedResult != expected) {
        std::cout << "  FAIL: Fixed result does not match expected" << std::endl;
        return false;
    }

    // Verify buggy result is indeed a huge number (underflow)
    if (buggyResult < 1000000000) {
        std::cout << "  FAIL: Buggy result should be > 1 billion (underflow)" << std::endl;
        return false;
    }

    std::cout << "  PASS: Fix correctly prevents underflow" << std::endl;
    return true;
}

/**
 * Test Case 3: Boundary case - maxClassNum == 499
 * Exactly at the threshold, no custom classes
 */
bool testBoundary_Equals499() {
    std::cout << "\n=== Test: Boundary Case (maxClassNum == 499) ===" << std::endl;

    duint32 maxClassNum = 499;
    duint32 expected = 0;  // No custom classes

    // Buggy version would give 0 too (499 - 499 = 0)
    duint32 buggyResult = calculateEndDataPos_Original(maxClassNum);
    duint32 fixedResult = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  Original: " << buggyResult << std::endl;
    std::cout << "  Fixed: " << fixedResult << std::endl;
    std::cout << "  Expected: " << expected << std::endl;

    if (fixedResult != expected) {
        std::cout << "  FAIL: Result does not match expected" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 4: Boundary case - maxClassNum == 500
 * Exactly one custom class
 */
bool testBoundary_Equals500() {
    std::cout << "\n=== Test: Boundary Case (maxClassNum == 500) ===" << std::endl;

    duint32 maxClassNum = 500;
    duint32 expected = 1;  // One custom class (class 500)

    duint32 result = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  Expected endDataPos = " << expected << std::endl;
    std::cout << "  Actual endDataPos = " << result << std::endl;

    if (result != expected) {
        std::cout << "  FAIL: Result does not match expected" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 5: Edge case - maxClassNum == 0
 * No classes at all
 */
bool testEdge_Zero() {
    std::cout << "\n=== Test: Edge Case (maxClassNum == 0) ===" << std::endl;

    duint32 maxClassNum = 0;
    duint32 expected = 0;

    // Buggy version would underflow massively
    duint32 buggyResult = calculateEndDataPos_Original(maxClassNum);
    duint32 fixedResult = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  BUGGY calculation: " << buggyResult << " (massive underflow!)" << std::endl;
    std::cout << "  FIXED calculation: " << fixedResult << std::endl;
    std::cout << "  Expected: " << expected << std::endl;

    if (fixedResult != expected) {
        std::cout << "  FAIL: Fixed result does not match expected" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 6: Edge case - maxClassNum == 498
 * One less than threshold
 */
bool testEdge_JustBelow() {
    std::cout << "\n=== Test: Edge Case (maxClassNum == 498) ===" << std::endl;

    duint32 maxClassNum = 498;
    duint32 expected = 0;

    // Buggy version would underflow
    duint32 buggyResult = calculateEndDataPos_Original(maxClassNum);
    duint32 fixedResult = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  BUGGY calculation: " << buggyResult << " (underflow!)" << std::endl;
    std::cout << "  FIXED calculation: " << fixedResult << std::endl;
    std::cout << "  Expected: " << expected << std::endl;

    if (fixedResult != expected) {
        std::cout << "  FAIL: Fixed result does not match expected" << std::endl;
        return false;
    }

    // Verify buggy result is indeed a huge number (underflow)
    if (buggyResult < 1000000000) {
        std::cout << "  FAIL: Buggy result should be > 1 billion (underflow)" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 7: Large value - maxClassNum == 1000
 * Many custom classes
 */
bool testLargeValue() {
    std::cout << "\n=== Test: Large Value (maxClassNum == 1000) ===" << std::endl;

    duint32 maxClassNum = 1000;
    duint32 expected = 501;  // 1000 - 499 = 501

    duint32 result = calculateEndDataPos_Fixed(maxClassNum);

    std::cout << "  maxClassNum = " << maxClassNum << std::endl;
    std::cout << "  Expected endDataPos = " << expected << std::endl;
    std::cout << "  Actual endDataPos = " << result << std::endl;

    if (result != expected) {
        std::cout << "  FAIL: Result does not match expected" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

/**
 * Test Case 8: Various AC1032 test values
 * Test with actual values seen in problematic files
 */
bool testAC1032Values() {
    std::cout << "\n=== Test: AC1032 Actual Values ===" << std::endl;

    // Values observed in AC1032 files from DWG TrueView
    duint32 testValues[] = {236, 240, 250, 300, 400, 450, 498};
    int numValues = sizeof(testValues) / sizeof(testValues[0]);

    bool allPassed = true;

    for (int i = 0; i < numValues; i++) {
        duint32 maxClassNum = testValues[i];
        duint32 expected = 0;  // All these are < 499

        duint32 result = calculateEndDataPos_Fixed(maxClassNum);

        std::cout << "  maxClassNum = " << maxClassNum
                  << " -> endDataPos = " << result
                  << " (expected " << expected << ")" << std::endl;

        if (result != expected) {
            std::cout << "    FAIL" << std::endl;
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "  PASS: All AC1032 values handled correctly" << std::endl;
    }

    return allPassed;
}

/**
 * Test Case 9: Consistency between original and fixed for valid values
 * For maxClassNum > 499, both should give same result
 */
bool testConsistency() {
    std::cout << "\n=== Test: Consistency (original == fixed for valid values) ===" << std::endl;

    bool allPassed = true;

    // Test various values where original code would work correctly
    for (duint32 maxClassNum = 500; maxClassNum <= 1000; maxClassNum += 50) {
        duint32 originalResult = calculateEndDataPos_Original(maxClassNum);
        duint32 fixedResult = calculateEndDataPos_Fixed(maxClassNum);

        if (originalResult != fixedResult) {
            std::cout << "  FAIL at maxClassNum = " << maxClassNum
                      << ": original=" << originalResult
                      << ", fixed=" << fixedResult << std::endl;
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "  PASS: Fixed code is consistent with original for valid inputs" << std::endl;
    }

    return allPassed;
}

int main(int argc, char* argv[]) {
    std::cout << "libdxfrw Class Count Underflow Tests" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Testing fix for integer underflow in maxClassNum calculation" << std::endl;
    std::cout << "Affected files: dwgreader18.cpp, dwgreader21.cpp" << std::endl;

    int failedTests = 0;
    int totalTests = 0;

    totalTests++;
    if (!testNormalCase()) failedTests++;

    totalTests++;
    if (!testBugCase_LessThan499()) failedTests++;

    totalTests++;
    if (!testBoundary_Equals499()) failedTests++;

    totalTests++;
    if (!testBoundary_Equals500()) failedTests++;

    totalTests++;
    if (!testEdge_Zero()) failedTests++;

    totalTests++;
    if (!testEdge_JustBelow()) failedTests++;

    totalTests++;
    if (!testLargeValue()) failedTests++;

    totalTests++;
    if (!testAC1032Values()) failedTests++;

    totalTests++;
    if (!testConsistency()) failedTests++;

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Tests: " << (totalTests - failedTests) << "/" << totalTests << " passed" << std::endl;

    if (failedTests > 0) {
        std::cout << "FAIL: " << failedTests << " test(s) failed" << std::endl;
        return 1;
    } else {
        std::cout << "SUCCESS: All underflow tests passed!" << std::endl;
        return 0;
    }
}
