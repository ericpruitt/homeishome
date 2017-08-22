#!/usr/bin/make -f
.POSIX:

INSTALLDIR = /usr/local/lib

CFLAGS = -Werror -Wall -fPIC -O3
CPPFLAGS = -D_GNU_SOURCE
LDFLAGS = -shared -ldl
LIBRARY_SO = homeishome.so

SANITY_CFLAGS = $(CFLAGS) -g -Weverything -flto -fsanitize=address,undefined
TEST_CFLAGS = -Werror -Wall -Wpedantic

all: $(LIBRARY_SO)

$(LIBRARY_SO): $(LIBRARY_SO:.so=.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $? -o $@ $(LDFLAGS)

sanity:
	$(MAKE) -s clean
	$(MAKE) all tests \
		CC="clang" \
		CFLAGS="$(CFLAGS) $(SANITY_CFLAGS)" \
		TEST_CFLAGS="$(TEST_CFLAGS) $(SANITY_CFLAGS)" \
	;
	$(MAKE) run-tests
	$(MAKE) clean

tests: tests.c
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $? -o $@

run-tests: $(LIBRARY_SO) tests
	LD_PRELOAD=$(PWD)/$(LIBRARY_SO) ./tests

$(INSTALLDIR)/$(LIBRARY_SO): $(LIBRARY_SO) tests
	cp $(LIBRARY_SO) $(INSTALLDIR)
	chmod 755 $(INSTALLDIR)/$(LIBRARY_SO)
	@LD_PRELOAD=$(INSTALLDIR)/$(LIBRARY_SO) ./tests >/dev/null
	@echo "$(INSTALLDIR)/$(LIBRARY_SO): installed, and all tests passed"

install: $(INSTALLDIR)/$(LIBRARY_SO)

uninstall:
	rm $(INSTALLDIR)/$(LIBRARY_SO)

clean:
	rm -f $(LIBRARY_SO) tests
