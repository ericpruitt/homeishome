/**
 * This library ensures that the "HOME" environment variable takes precedence
 * over paths returned by functions that query the password database.
 *
 * Metadata:
 * - Author:  Eric Pruitt; <https://www.codevat.com>
 * - License: [BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause)
 */
#include <dlfcn.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

const char service_interp[] __attribute__((section(".interp"))) = (
    LD_PATH
);

void lib_main(void);

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

// With the exception of "xstrdup", "_main" and "lib_main", functions below
// this line are thin wrappers around various library calls that return
// information from the password database. The wrappers invoke "alter_passwd"
// on any password database entries returned to the caller. The arguments
// accepted by these functions and the return values are identical to their
// canonical implementations.

struct passwd *getpwent(void)
{
    getpwent_type original = (getpwent_type) dlsym(RTLD_NEXT, "getpwent");
    return alter_passwd(original());
}

int getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen,
 struct passwd **pwbufp) {

    int result;
    getpwent_r_type original;

    original = (getpwent_r_type) dlsym(RTLD_NEXT, "getpwent_r");

    result = original(pwbuf, buf, buflen, pwbufp);
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
    getpwnam_r_type original;

    original = (getpwnam_r_type) dlsym(RTLD_NEXT, "getpwnam_r");

    result = original(name, pwbuf, buf, buflen, pwbufp);
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
    getpwuid_r_type original;

    original = (getpwuid_r_type) dlsym(RTLD_NEXT, "getpwuid_r");

    result = original(uid, pwbuf, buf, buflen, pwbufp);
    *pwbufp = alter_passwd(*pwbufp);
    return result;
}

//                                    ---

/**
 * This is a reimplementation of _strdup(3)_ that was created because the glibc
 * implementation of strdup triggers "disabled expansion of recursive macro
 * [-Werror,-Wdisabled-macro-expansion]" in Clang.
 */
static char *xstrdup(char *string)
{
    char *buffer = malloc(strlen(string) + 1);

    if (buffer) {
        strcpy(buffer, string);
    }

    return buffer;
}

/**
 * Read the command line for "/proc/self/cmdline" and generated values suitable
 * for use as "argc" and "argv" in a standard main function.
 *
 * Arguments:
 * - argc: Pointer to the destination where the number of arguments should be
 *   stored.
 * - argv: Pointer to the destination where the arguments should be stored. As
 *   with "argv" used for "main", this array is terminated with a NULL pointer.
 *
 * Returns: 0 if the arguments were successfully extracted or -1 otherwise with
 * errno set accordingly.
 */
static int cmdline(int *argc, char ***argv)
{
    ssize_t nread;
    FILE *stream;

    size_t len = 0;
    char *line = NULL;
    void *next = NULL;

    if (!(stream = fopen("/proc/self/cmdline", "r"))) {
        return -1;
    }

    for (*argc = 0; (nread = getdelim(&line, &len, '\0', stream)) != -1; ) {
        next = realloc(*argv, (size_t) ++(*argc) * sizeof(char *));

        if (!next) {
            for (; *argc > 0; (*argc)--) {
                free((*argv)[*argc - 1]);
            }

            free(*argv);
            return -1;
        }

        *argv = next;
        (*argv)[*argc - 1] = line;
        line = NULL;
        len = 0;
    }

    (*argv)[*argc] = NULL;
    return 0;
}

/**
 * This function is simply a renamed "main" function and works the same way.
 *
 * Arguments:
 * - argc: Number of command line arguments.
 * - argv: List of command line arguments terminated with a NULL pointer.
 *
 * Returns: The program exit status.
 */
static int _main(int argc, char **argv)
{
    char exe[PATH_MAX];
    char *path;
    char path_resolved[PATH_MAX];

    char *ld_preload = NULL;
    char *paths = NULL;


    if (argc < 2) {
        dprintf(STDERR_FILENO, "Usage: %s COMMAND [ARGUMENT]...\n", argv[0]);
    } else if (!realpath("/proc/self/exe", exe)) {
        perror("realpath: /proc/self/exe");
    } else {
        paths = getenv("LD_PRELOAD");

        if (!paths || paths[0] == '\0') {
            paths = NULL;
            ld_preload = xstrdup(exe);

            if (!ld_preload) {
                perror("xstrdup");
                goto error;
            }
        } else if (!(paths = xstrdup(paths))) {
            perror("xstrdup");
            goto error;
        } else {
            // Since strtok(3) may modify the underlying string, the new
            // LD_PRELOAD value is prepared in advance even though it might not
            // be used.
            ld_preload = malloc(strlen(paths) + /* ":" */ 1 + strlen(exe) + 1);

            if (!ld_preload) {
                perror("malloc");
                goto error;
            }

            stpcpy(stpcpy(stpcpy(ld_preload, paths), ":"), exe);

            for (path = strtok(paths, ":"); path; path = strtok(NULL, ":")) {
                if (!strcmp(exe, path) || (realpath(path, path_resolved) &&
                  !strcmp(exe, path_resolved))) {
                    goto exec;
                }
            }
        }

        if (setenv("LD_PRELOAD", ld_preload, 1)) {
            perror("setenv: LD_PRELOAD");
            goto error;
        }

exec:
        execvp(argv[1], argv + 1);
        perror("execvp");
    }

error:
    free(paths);
    free(ld_preload);
    _exit(255);
}

/**
 * This function is the entry point when the shared object is executed.
 */
void lib_main(void)
{
    int argc = 0;
    char **argv = NULL;
    int exit_status = 1;

    if (cmdline(&argc, &argv)) {
        perror("cmdline");
    } else {
        exit_status = _main(argc, argv);
    }

    exit(exit_status);
}
