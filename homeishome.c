/**
 * This library ensures that the "HOME" environment variable takes precedence
 * over paths returned by functions that query the password database.
 *
 * Metadata:
 * - Author:  Eric Pruitt; <https://www.codevat.com>
 * - License: [BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause)
 */
#include <dlfcn.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

struct passwd *alter_passwd(struct passwd *);

/**
 * Function pointer types of the canonical implementations of the overridden
 * functions.
 */
typedef struct passwd *(*getpwent_type)(void);
typedef int (*getpwent_r_type)(struct passwd *, char *, size_t,
                               struct passwd **);
typedef struct passwd *(*getpwnam_type)(const char *);
typedef int (*getpwnam_r_type)(const char *, struct passwd *, char *, size_t,
                               struct passwd **);
typedef struct passwd *(*getpwuid_type)(uid_t);
typedef int (*getpwuid_r_type)(uid_t, struct passwd *, char *, size_t,
                               struct passwd **);

/**
 * If a password database entry represents the current user and the "HOME"
 * environment variable is a non-empty string, ensure that the variable's value
 * is used as the home directory (`entry->pw_dir = getenv("HOME")`).
 *
 * Arguments:
 * - entry: Password database entry.
 *
 * Returns: Potentially modified password database entry.
 */
struct passwd *alter_passwd(struct passwd *entry)
{
    char *home = getenv("HOME");

    if (entry && entry->pw_uid == geteuid() && home && home[0] != '\0') {
        entry->pw_dir = home;
    }

    return entry;
}

// All functions below this line are thin wrappers around various library calls
// that return information about the password database that invoke
// "alter_passwd". The arguments accepted by these functions and the return
// values are identical to their canonical implementations.

struct passwd *getpwent(void)
{
    getpwent_type original = (getpwent_type) dlsym(RTLD_NEXT, "getpwent");
    return alter_passwd(original());
}

int getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen,
 struct passwd **pwbufp) {

    int result;
    getpwent_r_type original_getpwent_r;

    original_getpwent_r = (getpwent_r_type) dlsym(RTLD_NEXT, "getpwent_r");

    result = original_getpwent_r(pwbuf, buf, buflen, pwbufp);
    *pwbufp = alter_passwd(*pwbufp);
    return result;
}

struct passwd *getpwnam(const char *name)
{
    getpwnam_type original = (getpwnam_type) dlsym(RTLD_NEXT, "getpwnam");
    return alter_passwd(original(name));
}

int getpwnam_r(const char *name, struct passwd *pwbuf, char *buf,
  size_t buflen, struct passwd **pwbufp) {

    int result;
    getpwnam_r_type original_getpwnam_r;

    original_getpwnam_r = (getpwnam_r_type) dlsym(RTLD_NEXT, "getpwnam_r");

    result = original_getpwnam_r(name, pwbuf, buf, buflen, pwbufp);
    *pwbufp = alter_passwd(*pwbufp);
    return result;
}

struct passwd *getpwuid(uid_t uid)
{
    getpwuid_type original = (getpwuid_type) dlsym(RTLD_NEXT, "getpwuid");
    return alter_passwd(original(uid));
}

int getpwuid_r(uid_t uid, struct passwd *pwbuf, char *buf, size_t buflen,
  struct passwd **pwbufp) {

    int result;
    getpwuid_r_type original_getpwuid_r;

    original_getpwuid_r = (getpwuid_r_type) dlsym(RTLD_NEXT, "getpwuid_r");

    result = original_getpwuid_r(uid, pwbuf, buf, buflen, pwbufp);
    *pwbufp = alter_passwd(*pwbufp);
    return result;
}
