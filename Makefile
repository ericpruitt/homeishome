#!/usr/bin/make -f
.POSIX:

INSTALLDIR = /usr/local/bin

CFLAGS = -Wall -fPIC -O3
CPPFLAGS = -D_GNU_SOURCE
LDFLAGS = -shared -ldl -Wl,-e,main
LIBRARY_SO = homeishome.so

TEST_CFLAGS = -Werror -Wall -Wpedantic

all: $(LIBRARY_SO)

.c:
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c

config.h: noop
	@rm -f $@.tmp
	@ld_path="$$( \
		export LC_ALL=C && \
		readelf -l $? \
		| sed -n 's/.*\[Requesting program interpreter: \(.*\)\]/\1/p'\
	)"; \
	if [ -z "$$ld_path" ]; then \
		echo "$@: unable to determine ELF interpreter path" >&2; \
		exit 1; \
	fi; \
	printf '#define ELF_INTERP "%s"\n' "$$ld_path" | tee -a $@.tmp
	@mv $@.tmp $@

executableso.o: config.h

$(LIBRARY_SO): homeishome.o executableso.o
	$(CC) -o $@ $(LDFLAGS) *.o

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
	rm -f *.o *.so config.h noop tests
