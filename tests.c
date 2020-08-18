/**
 * Test suite for homeishome.so. The compiled binary accepts no arguments. Log
 * messages for failing tests are written to standard error, and messages for
 * other tests are written to standard output.
 *
 * Exit Statuses:
 * - 0: All tests passed.
 * - 1: An error occurred during initialization.
 * - 2: One or more tests failed.
 *
 * Metadata:
 * - Author:  Eric Pruitt; <https://www.codevat.com>
 * - License: [BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause)
 */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

/**
 * This value is used to check whether or not `passwd->pw_dir` has been
 * modified.
 */
#define HOME_CANARY "XXX"

/**
 * VT100 escape sequence to reset terminal attributes.
 */
#define RESET_ATTRIBUTES "\033[m"

/**
 * VT100 escape sequence to make the foreground green.
 */
#define SETAF_GREEN "\033[92m"

/**
 * VT100 escape sequence to make the foreground red.
 */
#define SETAF_RED "\033[91m"

/**
 * VT100 escape sequence to make the foreground yellow.
 */
#define SETAF_YELLOW "\033[93m"

struct passwd* check_entry(const char *, struct passwd *);

/**
 * Number of checks in "check_entry" that failed.
 */
static size_t failures = 0;

/**
 * Check a password database entry and verify that the home directory has
 * correctly been overridden or left unmodified. A log message summarizing the
 * result of the check is shown. The message is written to standard error if
 * the test failed and standard output otherwise.
 *
 * Arguments:
 * - prefix: This text is prepended to the log message.
 * - entry: Password database entry returned or set by a function.
 *
 * Returns: The value of "entry", unmodified.
 */
struct passwd* check_entry(const char *prefix, struct passwd *entry)
{
    #define LOG_STREAM (passed || ignoreable ? stdout : stderr)
    #define STATUS ( \
            passed ? SETAF_GREEN "PASS" RESET_ATTRIBUTES : \
        ignoreable ? SETAF_YELLOW "IGNORED" RESET_ATTRIBUTES : \
                     SETAF_RED "FAIL" RESET_ATTRIBUTES \
    )

    bool canary_found;

    int _errno = errno;
    bool canary_expected = (entry && geteuid() == entry->pw_uid);
    bool passed = false;

    // Some tests used for negative assertions use the superuser account as the
    // alternate test subject. Since these will always fail when running as
    // root, they are ignored.
    bool ignoreable = entry && !entry->pw_uid && !geteuid();

    if (entry) {
        canary_found = entry->pw_dir && !strcmp(entry->pw_dir, HOME_CANARY);
        passed = (canary_expected == canary_found);

        fprintf(LOG_STREAM,
            "%s: %s: pw_uid = %u (%s), pw_dir = %s\n", prefix, STATUS,
            entry->pw_uid,
            entry->pw_name,
            entry->pw_dir
        );
    } else if (passed || !_errno) {
        fprintf(LOG_STREAM, "%s: %s: *entry is NULL\n", prefix, STATUS);
    } else {
        fprintf(LOG_STREAM, "%s: %s: *entry is NULL: %s\n", prefix, STATUS,
            strerror(_errno)
        );
    }

    if (!passed && !ignoreable) {
        failures++;
    }

    return entry;
    #undef LOG_STREAM
    #undef STATUS
}

int main(void)
{
    struct passwd *entry, pwd;
    char *logname;
    int status = 0;

    size_t buflen = (size_t) sysconf(_SC_GETPW_R_SIZE_MAX);
    char *buf = NULL, *superuser_name = NULL;

    if (!(logname = getenv("LOGNAME")) || logname[0] == '\0') {
        fputs("tests: LOGNAME must be the current user's name\n", stderr);
        return 1;
    }

    if (setenv("HOME", HOME_CANARY, 1)) {
        perror("setenv");
        status = 1;
        goto clean_up;
    }

    if (!(buf = malloc(buflen))) {
        perror("malloc");
        status = 1;
        goto clean_up;
    }

    if (!(errno = 0, entry = getpwuid(0))) {
        if (!errno) {
            errno = EINVAL;
        }
        perror("getpwuid(0)");
        status = 1;
        goto clean_up;
    }

    // The glibc implementation of strdup triggers "disabled expansion of
    // recursive macro [-Werror,-Wdisabled-macro-expansion]" in Clang.
    if ((superuser_name = malloc(strlen(entry->pw_name) + 1))) {
        strcpy(superuser_name, entry->pw_name);
    } else {
        perror("malloc");
        status = 1;
        goto clean_up;
    }

    setpwent();
    for (; errno = 0, (entry = getpwent()); check_entry("getpwent", entry));
    endpwent();

    setpwent();
    for (; errno = 0, getpwent_r(&pwd, buf, buflen, &entry), entry; ) {
        check_entry("getpwent_r", entry);
    }
    endpwent();

    errno = 0;
    check_entry("getpwnam", getpwnam(logname));

    errno = 0;
    getpwnam_r(logname, &pwd, buf, buflen, &entry);
    check_entry("getpwnam_r", entry);

    errno = 0;
    getpwnam_r(superuser_name, &pwd, buf, buflen, &entry);
    check_entry("getpwnam_r", entry);

    errno = 0;
    check_entry("getpwuid", getpwuid(geteuid()));

    errno = 0;
    getpwuid_r(geteuid(), &pwd, buf, buflen, &entry);
    check_entry("getpwuid_r", entry);

    errno = 0;
    getpwuid_r(0, &pwd, buf, buflen, &entry);
    check_entry("getpwuid_r", entry);

clean_up:
    free(buf);
    free(superuser_name);

    if (status) {
        return status;
    }

    printf("Failures: %s%zu" RESET_ATTRIBUTES "\n",
        failures ? SETAF_RED : SETAF_GREEN,
        failures
    );

    return failures ? 2 : 0;
}
