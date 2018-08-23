#!/usr/bin/make -f
.POSIX:

INSTALLDIR = /usr/local/bin

# TODO: Figure out how to use the address and undefined behavior sanitizer with
# Clang.
CFLAGS = -Wall -fPIC -O3
CPPFLAGS = -D_GNU_SOURCE
LDFLAGS = -ldl
LIBRARY_SO = homeishome.so

SANITY_CFLAGS = $(CFLAGS) -g -fsanitize=address,undefined
TEST_CFLAGS = -Werror -Wall -Wpedantic
LIBASAN_SO = /usr/lib/gcc/x86_64-linux-gnu/6/libasan.so

all: $(LIBRARY_SO)

$(LIBRARY_SO): $(LIBRARY_SO:.so=.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $? -o $@ $(LDFLAGS)

sanity:
	$(MAKE) -s clean
	$(MAKE) tests
	$(MAKE) \
		CC="gcc" \
		CFLAGS="$(CFLAGS) $(SANITY_CFLAGS)" \
		TEST_CFLAGS="$(TEST_CFLAGS) $(SANITY_CFLAGS)" \
		LDFLAGS="$(LDFLAGS) -lasan -lubsan" \
	;
	$(MAKE) run-tests
	$(MAKE) clean

tests: tests.c
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $? -o $@

run-tests: $(LIBRARY_SO) tests
	LD_PRELOAD="$(LIBASAN_SO)" ./$(LIBRARY_SO) ./tests

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
	rm -f $(LIBRARY_SO) tests
