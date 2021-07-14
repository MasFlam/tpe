ifndef profile
	profile := release
endif

CFLAGS := -D_POSIX_C_SOURCE=200809L -std=c99
LDFLAGS := -lnotcurses-core -lm

ifeq "$(profile)" "release"
	CFLAGS += -O2
else
	CFLAGS += -g
endif

.PHONY: clean help

tpe: tpe.c
	cc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f tpe

help:
	@echo "Usage:"
	@echo "  make [profile=<profile>] [<target>]"
	@echo "Available targets:"
	@echo "  tpe   -- build tpe (default)"
	@echo "  clean -- remove tpe executable"
	@echo "  help  -- show help"
	@echo "Available profiles:"
	@echo "  release -- build optimized binary (default)"
	@echo "  debug   -- build binary for debugging"
