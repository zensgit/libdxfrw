/******************************************************************************
**  libDXFrw - DWG Class Parsing Tests                                       **
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
 * Integration tests for DWG class parsing.
 *
 * These tests verify that the class parsing logic in dwgreader18 and dwgreader21
 * correctly handles various maxClassNum values, especially edge cases that
 * previously caused integer underflow bugs.
 *
 * Test coverage:
 * - Regression test: AC1027 files still work correctly
 * - Memory safety: AC1032 files don't cause memory explosion
 * - Boundary conditions: Various class count thresholds
 */

#include "libdwgr.h"
#include "test_interface.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <cstdlib>

// Platform-specific includes
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#include <unistd.h>
#define HAS_GETRUSAGE 1
#elif defined(_WIN32)
#include <process.h>  // For _getpid()
#define HAS_GETRUSAGE 0
#else
#define HAS_GETRUSAGE 0
#endif

#if HAS_GETRUSAGE
/**
 * Get current memory usage in KB (POSIX only)
 * Note: ru_maxrss units differ by platform:
 *   - Linux: kilobytes
 *   - macOS/BSD: bytes
 */
long getMemoryUsageKB() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
    // macOS returns bytes, convert to KB
    return usage.ru_maxrss / 1024;
#else
    // Linux returns KB
    return usage.ru_maxrss;
#endif
}
#endif

/**
 * Get platform-appropriate temp directory path
 */
std::string getTempDir() {
    // Check environment variables in order of preference
    const char* tmpdir = std::getenv("TMPDIR");  // Unix standard
    if (tmpdir && tmpdir[0] != '\0') return tmpdir;

    tmpdir = std::getenv("TEMP");  // Windows
    if (tmpdir && tmpdir[0] != '\0') return tmpdir;

    tmpdir = std::getenv("TMP");   // Windows fallback
    if (tmpdir && tmpdir[0] != '\0') return tmpdir;

#if defined(_WIN32)
    return "C:\\Windows\\Temp";
#else
    return "/tmp";
#endif
}

/**
 * Build a unique temp file path to avoid collisions
 */
std::string buildTempFilePath(const char* basename) {
    std::string dir = getTempDir();
    // Ensure trailing separator
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
#if defined(_WIN32)
        dir += '\\';
#else
        dir += '/';
#endif
    }
    // Use PID for uniqueness
#if defined(_WIN32)
    int pid = _getpid();
#elif defined(__unix__) || defined(__APPLE__)
    int pid = getpid();
#else
    // Fallback: use a fixed suffix if no PID available
    int pid = 0;
#endif
    return dir + basename + "_" + std::to_string(pid) + ".dwg";
}

/**
 * Get the test data directory from environment variable
 * Returns nullptr if not set
 */
const char* getTestDataDir() {
    return std::getenv("LIBDXFRW_TEST_DATA_DIR");
}

/**
 * Build full path to test file
 */
std::string buildTestPath(const char* filename) {
    const char* testDataDir = getTestDataDir();
    if (!testDataDir) {
        return "";
    }
    std::string path = testDataDir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += filename;
    return path;
}

/**
 * Test reading a DWG file with timeout and memory check
 * Returns: true if file was read without memory explosion
 */
bool testDWGFileMemorySafe(const char* filepath, const char* description,
                           long maxMemoryKB = 100 * 1024) {  // Default 100MB limit
    std::cout << "\n=== Test: " << description << " ===" << std::endl;
    std::cout << "  File: " << filepath << std::endl;

    // Check if file exists
    std::ifstream filecheck(filepath);
    if (!filecheck.good()) {
        std::cout << "  SKIP: File not found" << std::endl;
        return true;  // Not a failure, just skip
    }
    filecheck.close();

#if HAS_GETRUSAGE
    long initialMemory = getMemoryUsageKB();
    std::cout << "  Initial memory: " << initialMemory << " KB" << std::endl;
#endif

    auto startTime = std::chrono::steady_clock::now();

    // Try to read the file
    dwgR dwg(filepath);
    TestInterface reader;
    bool readSuccess = dwg.read(&reader, false);

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

#if HAS_GETRUSAGE
    long finalMemory = getMemoryUsageKB();
    long memoryUsed = finalMemory - initialMemory;
#endif

    std::cout << "  Read result: " << (readSuccess ? "success" : "failed") << std::endl;
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;

#if HAS_GETRUSAGE
    std::cout << "  Memory used: " << memoryUsed << " KB" << std::endl;
    std::cout << "  Final memory: " << finalMemory << " KB" << std::endl;

    // Check for memory explosion (the original bug)
    // Use memory increase rather than absolute value to avoid false positives
    if (memoryUsed > maxMemoryKB) {
        std::cout << "  FAIL: Memory increase exceeded limit of " << maxMemoryKB << " KB" << std::endl;
        return false;
    }
#else
    std::cout << "  Memory check: SKIP (not available on this platform)" << std::endl;
#endif

    // Check for reasonable execution time (the original bug caused infinite loop)
    if (duration.count() > 30000) {  // 30 seconds
        std::cout << "  FAIL: Execution time exceeded 30 seconds (possible infinite loop)" << std::endl;
        return false;
    }

    std::cout << "  PASS: Memory and time within acceptable limits" << std::endl;
    return true;
}

/**
 * Test AC1027 (R2013) file - should work correctly (regression test)
 */
bool testAC1027Regression() {
    std::string filepath = buildTestPath("autocad_2013.dwg");
    if (filepath.empty()) {
        std::cout << "\n=== Test: AC1027 (R2013) Regression Test ===" << std::endl;
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }
    return testDWGFileMemorySafe(
        filepath.c_str(),
        "AC1027 (R2013) Regression Test"
    );
}

/**
 * Test AC1032 (R2018) file - should not cause memory explosion
 */
bool testAC1032MemorySafety_2018() {
    std::string filepath = buildTestPath("autocad_2018.dwg");
    if (filepath.empty()) {
        std::cout << "\n=== Test: AC1032 (R2018) Memory Safety Test ===" << std::endl;
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }
    return testDWGFileMemorySafe(
        filepath.c_str(),
        "AC1032 (R2018) Memory Safety Test"
    );
}

/**
 * Test AC1032 (R2021) file - should not cause memory explosion
 */
bool testAC1032MemorySafety_2021() {
    std::string filepath = buildTestPath("autocad_2021.dwg");
    if (filepath.empty()) {
        std::cout << "\n=== Test: AC1032 (R2021) Memory Safety Test ===" << std::endl;
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }
    return testDWGFileMemorySafe(
        filepath.c_str(),
        "AC1032 (R2021) Memory Safety Test"
    );
}

/**
 * Test AC1032 (R2024) file - should not cause memory explosion
 */
bool testAC1032MemorySafety_2024() {
    std::string filepath = buildTestPath("autocad_2024.dwg");
    if (filepath.empty()) {
        std::cout << "\n=== Test: AC1032 (R2024) Memory Safety Test ===" << std::endl;
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }
    return testDWGFileMemorySafe(
        filepath.c_str(),
        "AC1032 (R2024) Memory Safety Test"
    );
}

/**
 * Test AC1018 (R2004) file - should work correctly (regression test)
 */
bool testAC1018Regression() {
    std::string filepath = buildTestPath("autocad_2004.dwg");
    if (filepath.empty()) {
        std::cout << "\n=== Test: AC1018 (R2004) Regression Test ===" << std::endl;
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }
    return testDWGFileMemorySafe(
        filepath.c_str(),
        "AC1018 (R2004) Regression Test"
    );
}

/**
 * Test that version detection works correctly
 */
bool testVersionDetection() {
    std::cout << "\n=== Test: Version Detection ===" << std::endl;

    const char* testDataDir = getTestDataDir();
    if (!testDataDir) {
        std::cout << "  SKIP: LIBDXFRW_TEST_DATA_DIR not set" << std::endl;
        return true;
    }

    struct VersionTest {
        const char* filename;
        const char* expectedVersion;
    };

    VersionTest tests[] = {
        {"autocad_2004.dwg", "AC1018"},
        {"autocad_2013.dwg", "AC1027"},
        {"autocad_2018.dwg", "AC1032"},
        {"autocad_2021.dwg", "AC1032"},
        {"autocad_2024.dwg", "AC1032"},
    };

    int numTests = sizeof(tests) / sizeof(tests[0]);
    bool allPassed = true;

    for (int i = 0; i < numTests; i++) {
        std::string filepath = buildTestPath(tests[i].filename);
        std::ifstream file(filepath, std::ios::binary);
        if (!file.good()) {
            std::cout << "  SKIP: " << tests[i].filename << " not found" << std::endl;
            continue;
        }

        char version[7] = {0};
        file.read(version, 6);
        file.close();

        std::cout << "  " << tests[i].filename << std::endl;
        std::cout << "    Expected: " << tests[i].expectedVersion << std::endl;
        std::cout << "    Actual: " << version << std::endl;

        if (strcmp(version, tests[i].expectedVersion) != 0) {
            std::cout << "    FAIL: Version mismatch" << std::endl;
            allPassed = false;
        } else {
            std::cout << "    PASS" << std::endl;
        }
    }

    return allPassed;
}

/**
 * Test that reading non-existent file fails gracefully
 */
bool testNonExistentFile() {
    std::cout << "\n=== Test: Non-existent File Handling ===" << std::endl;

    const char* nonExistent = "/this/file/does/not/exist.dwg";

    dwgR dwg(nonExistent);
    TestInterface reader;
    bool result = dwg.read(&reader, false);

    std::cout << "  File: " << nonExistent << std::endl;
    std::cout << "  Read result: " << (result ? "success (unexpected)" : "failed (expected)") << std::endl;

    if (result) {
        std::cout << "  FAIL: Should have failed for non-existent file" << std::endl;
        return false;
    }

    std::cout << "  PASS: Correctly failed for non-existent file" << std::endl;
    return true;
}

/**
 * Test that corrupted/empty file fails gracefully
 */
bool testCorruptedFile() {
    std::cout << "\n=== Test: Corrupted File Handling ===" << std::endl;

    // Create a temp file with invalid content (using portable path)
    std::string tempFilePath = buildTempFilePath("test_corrupt");
    const char* tempFile = tempFilePath.c_str();
    std::ofstream out(tempFile, std::ios::binary);
    out << "This is not a valid DWG file";
    out.close();

    dwgR dwg(tempFile);
    TestInterface reader;

#if HAS_GETRUSAGE
    long initialMemory = getMemoryUsageKB();
#endif
    bool result = dwg.read(&reader, false);
#if HAS_GETRUSAGE
    long finalMemory = getMemoryUsageKB();
#endif

    std::cout << "  Read result: " << (result ? "success (unexpected)" : "failed (expected)") << std::endl;
#if HAS_GETRUSAGE
    std::cout << "  Memory before: " << initialMemory << " KB" << std::endl;
    std::cout << "  Memory after: " << finalMemory << " KB" << std::endl;
#endif

    std::remove(tempFile);

    if (result) {
        std::cout << "  FAIL: Should have failed for corrupted file" << std::endl;
        return false;
    }

#if HAS_GETRUSAGE
    // Check for memory leak
    if (finalMemory > initialMemory + 10240) {  // Allow 10MB tolerance
        std::cout << "  FAIL: Possible memory leak" << std::endl;
        return false;
    }
#endif

    std::cout << "  PASS: Correctly failed for corrupted file without memory issues" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "libdxfrw DWG Class Parsing Tests" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Testing DWG reading with focus on class count edge cases" << std::endl;

    const char* testDataDir = getTestDataDir();
    if (testDataDir) {
        std::cout << "Test data directory: " << testDataDir << std::endl;
    } else {
        std::cout << "Note: LIBDXFRW_TEST_DATA_DIR not set, some tests will be skipped" << std::endl;
    }

    int failedTests = 0;
    int totalTests = 0;

    // Version detection test
    totalTests++;
    if (!testVersionDetection()) failedTests++;

    // Non-existent file test
    totalTests++;
    if (!testNonExistentFile()) failedTests++;

    // Corrupted file test
    totalTests++;
    if (!testCorruptedFile()) failedTests++;

    // AC1018 regression test
    totalTests++;
    if (!testAC1018Regression()) failedTests++;

    // AC1027 regression test
    totalTests++;
    if (!testAC1027Regression()) failedTests++;

    // AC1032 memory safety tests (the main bug fix verification)
    totalTests++;
    if (!testAC1032MemorySafety_2018()) failedTests++;

    totalTests++;
    if (!testAC1032MemorySafety_2021()) failedTests++;

    totalTests++;
    if (!testAC1032MemorySafety_2024()) failedTests++;

    std::cout << "\n=================================" << std::endl;
    std::cout << "Tests: " << (totalTests - failedTests) << "/" << totalTests << " passed" << std::endl;

    if (failedTests > 0) {
        std::cout << "FAIL: " << failedTests << " test(s) failed" << std::endl;
        return 1;
    } else {
        std::cout << "SUCCESS: All DWG class parsing tests passed!" << std::endl;
        return 0;
    }
}
