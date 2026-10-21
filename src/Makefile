#
# Delegating Makefile for src/
# Forwards build and management targets to root Makefile
#

.PHONY: all clean run test install uninstall

all:
	@$(MAKE) -C .. all

clean:
	@$(MAKE) -C .. clean

run:
	@$(MAKE) -C .. run

test:
	@$(MAKE) -C .. test

install:
	@$(MAKE) -C .. install

uninstall:
	@$(MAKE) -C .. uninstall

%:
	@$(MAKE) -C .. $@
