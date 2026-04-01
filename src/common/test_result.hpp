#pragma once

/**
 * @brief Generic selftest results.
 */
typedef enum {
    TestResult_Unknown = 0,
    TestResult_Skipped = 1,
    TestResult_Passed = 2,
    TestResult_Failed = 3,
} TestResult;

/**
 * @brief Selftest results for a network interface.
 */
typedef enum {
    TestResultNet_Unknown = 0, // test did not run
    TestResultNet_Unlinked = 1, // wifi not present, eth cable unplugged
    TestResultNet_Down = 2, // wifi present, eth cable plugged, not selected in lan settings
    TestResultNet_NoAddress = 3, // wifi present, no address obtained from DHCP
    TestResultNet_Up = 4, // wifi present, eth cable plugged, selected in lan settings
} TestResultNet;
