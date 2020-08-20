#!/usr/bin/make -f
.POSIX:

INSTALLDIR = /usr/local/bin

CFLAGS = -Wall -fPIC -O3 -shared
CPPFLAGS = -D_GNU_SOURCE
LDFLAGS = -ldl -Wl,-e,homeishome_so_main
LIBRARY_SO = homeishome.so

TEST_CFLAGS = -Werror -Wall -Wpedantic

all: $(LIBRARY_SO)

noop: noop.c
	$(CC) $? -o $@

config.h: noop
	rm -f $@.tmp
	@ldd $? | awk >> $@.tmp ' \
		/\/ld/ { \
			if (hits++) { \
				print "$@: multiple ld matches" > "/dev/fd/2";\
				exit; \
			} \
			print "#define LD_PATH \"" $$(NF - 1) "\""; \
		} \
		END { \
			if (hits == 0) {; \
				print "no ld matches" > "/dev/fd/2"; \
			} \
			exit (hits == 1 ? 0 : 1); \
		} \
	'
	mv $@.tmp $@

$(LIBRARY_SO): homeishome.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) homeishome.c -o $@ $(LDFLAGS)

tests: tests.c
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $? -o $@

test: $(LIBRARY_SO) tests
	./$(LIBRARY_SO) ./tests

$(INSTALLDIR)/$(LIBRARY_SO): $(LIBRARY_SO) tests
	cp $(LIBRARY_SO) $(INSTALLDIR)
	chmod 755 $(INSTALLDIR)/$(LIBRARY_SO)
	@LD_PRELOAD=$(INSTALLDIR)/$(LIBRARY_SO) ./tests >/dev/null
	@$@ ./tests >/dev/null
	@echo "$(INSTALLDIR)/$(LIBRARY_SO): installed, and all tests passed"

install: $(INSTALLDIR)/$(LIBRARY_SO)

uninstall:
	rm $(INSTALLDIR)/$(LIBRARY_SO)

clean:
	rm -f $(LIBRARY_SO) config.h noop tests
