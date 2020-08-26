/**
 * When loaded via "LD_PRELOAD", this library ensures that the "HOME"
 * environment variable takes precedence over paths returned by functions that
 * query the password database. The library supplies wrappers for the following
 * functions:
 *
 * - _getpwent(3)_
 * - _getpwent_r(3)_
 * - _getpwnam(3)_
 * - _getpwnam_r(3)_
 * - _getpwuid(3)_
 * - _getpwuid_r(3)_
 *
 * Metadata:
 * - Author:  Eric Pruitt; <https://www.codevat.com>
 * - License: [BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause)
 */
#include <dlfcn.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * If a password database entry represents the current user and the "HOME"
 * environment variable is a non-empty string, ensure that the "HOME"
 * variable's value is used as the home directory (`entry->pw_dir =
 * getenv("HOME")`).
 *
 * Arguments:
 * - entry: Password database entry.
 *
 * Returns: Potentially modified password database entry.
 */
static struct passwd *alter_passwd(struct passwd *entry)
{
    char *home = getenv("HOME");

    if (entry && entry->pw_uid == geteuid() && home && home[0] != '\0') {
        entry->pw_dir = home;
    }

    return entry;
}

/**
 * Works almost the same as _dlsym(3)_, but if the underlying _dlsym(3)_ call
 * fails, a diagnostic message is written to standard error.
 *
 * Arguments:
 * - handle: See _dlsym(3)_.
 * - symbol: See _dlsym(3)_.
 *
 * Returns: On success, this function returns the address associated with
 * symbol, but on failure, `NULL` is returned.
 */
static void *xdlsym(void *handle, const char *symbol)
{
    void *result;

    if (!(result = dlsym(handle, symbol))) {
        dprintf(STDERR_FILENO, "dlsym(..., \"%s\"): %s\n", symbol, dlerror());
    }

    return result;
}

// The functions in this section are thin wrappers around various library calls
// that return information from the password database. The wrappers invoke
// "alter_passwd" on any password database entries before returning them to the
// caller. The arguments accepted by these functions and the return values are
// identical to their canonical implementations.

struct passwd *getpwent(void)
{
    struct passwd *(*fn)(void);

    fn = xdlsym(RTLD_NEXT, "getpwent");
    return alter_passwd(fn());
}

int getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen,
  struct passwd **pwbufp)
{
    int (*fn)(struct passwd *, char *, size_t, struct passwd **);
    int result;

    fn = xdlsym(RTLD_NEXT, "getpwent_r");
    result = fn(pwbuf, buf, buflen, pwbufp);
    alter_passwd(*pwbufp);
    return result;
}

struct passwd *getpwnam(const char *name)
{
    struct passwd *(*fn)(const char *);

    fn = xdlsym(RTLD_NEXT, "getpwnam");
    return alter_passwd(fn(name));
}

int getpwnam_r(const char *name, struct passwd *pwbuf, char *buf,
  size_t buflen, struct passwd **pwbufp)
{
    int (*fn)(const char *, struct passwd *, char *, size_t, struct passwd **);
    int result;

    fn = xdlsym(RTLD_NEXT, "getpwnam_r");
    result = fn(name, pwbuf, buf, buflen, pwbufp);
    alter_passwd(*pwbufp);
    return result;
}

struct passwd *getpwuid(uid_t uid)
{
    struct passwd *(*fn)(uid_t);

    fn = xdlsym(RTLD_NEXT, "getpwuid");
    return alter_passwd(fn(uid));
}

int getpwuid_r(uid_t uid, struct passwd *pwbuf, char *buf, size_t buflen,
  struct passwd **pwbufp)
{
    int (*fn)(uid_t, struct passwd *, char *, size_t, struct passwd **);
    int result;

    fn = xdlsym(RTLD_NEXT, "getpwuid_r");
    result = fn(uid, pwbuf, buf, buflen, pwbufp);
    alter_passwd(*pwbufp);
    return result;
}
