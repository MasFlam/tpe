CFLAGS := -D_POSIX_C_SOURCE=200809L -std=c99 -Wall -Wextra -pedantic
LDFLAGS := -lnotcurses-core -lm

.PHONY: clean

tpe: tpe.c
	cc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f tpe