#!/usr/bin/make -f
.POSIX:

BIN = /usr/local/bin
LIB = /usr/local/lib

CFLAGS = -Wall -fPIC -O3
CPPFLAGS = -D_GNU_SOURCE
LDFLAGS = -shared -ldl

TEST_CFLAGS = -Werror -Wall -Wpedantic

all: homeishome

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

homeishome.so: homeishome.o
	$(CC) -o $@ $? $(LDFLAGS)

homeishome: homeishome.o executableso.o
	$(CC) -o $@ *.o $(LDFLAGS)

tests: tests.c
	$(CC) -o $@ $(CPPFLAGS) $(TEST_CFLAGS) $?

test: tests
	@if ! [ -e homeishome ] && ! [ -e homeishome.so ]; then \
		echo "$@: no compiled objects available for testing" >&2; \
		exit 1; \
	fi; \
	if [ -e homeishome ]; then \
		./homeishome ./tests || trap 'exit 1' EXIT; \
		LD_PRELOAD=$$PWD/homeishome ./tests || trap 'exit 1' EXIT; \
	fi; \
	if [ -e homeishome.so ]; then \
		LD_PRELOAD=$$PWD/homeishome.so ./tests || trap 'exit 1' EXIT; \
	fi

clean:
	rm -f config.h homeishome noop tests *.o *.so

$(BIN)/homeishome: homeishome
	install -m 755 $? $@

$(LIB)/homeishome.so: homeishome.so
	install -m 644 $? $@

install:
	@if ! [ -e homeishome ] && ! [ -e homeishome.so ]; then \
		echo "$@: no compiled objects available to install" >&2; \
		exit 1; \
	fi
	@if [ -e homeishome ]; then \
		$(MAKE) $(BIN)/homeishome; \
	fi
	@if [ -e homeishome.so ]; then \
		$(MAKE) $(LIB)/homeishome.so; \
	fi

uninstall:
	rm -f $(BIN)/homeishome $(LIB)/homeishome.so
