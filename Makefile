CC      ?= cc
CFLAGS  ?= -Wall -Wextra -O2
PREFIX  ?= /usr/local

SBINDIR  = $(PREFIX)/sbin
SHAREDIR = $(PREFIX)/share/quartermaster
RULESDIR = $(SHAREDIR)/rules
CONFDIR  = /etc/quartermaster.d

DAEMON_SRC = quartermaster.c quartermaster-config.c quartermaster-state.c quartermaster-commands.c
DAEMON_HDR = quartermaster-config.h quartermaster-state.h quartermaster-commands.h

all: quartermaster quartermaster-notify

quartermaster: $(DAEMON_SRC) $(DAEMON_HDR)
	$(CC) $(CFLAGS) -o $@ $(DAEMON_SRC)

quartermaster-notify: quartermaster-notify.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	install -Dm755 quartermaster        $(DESTDIR)$(SBINDIR)/quartermaster
	install -Dm755 quartermaster-notify $(DESTDIR)$(SBINDIR)/quartermaster-notify
	install -dm755 $(DESTDIR)$(CONFDIR)
	install -dm755 $(DESTDIR)$(RULESDIR)
	for f in examples/*.conf; do \
		install -Dm644 "$$f" "$(DESTDIR)$(CONFDIR)/$$(basename "$$f")"; \
	done
	for f in examples/*.rules; do \
		install -Dm644 "$$f" "$(DESTDIR)$(RULESDIR)/$$(basename "$$f")"; \
	done
	@printf '\nquartermaster installed.\n\n'
	@printf 'Nothing is active yet. To enable a device:\n'
	@printf '  quartermaster list\n'
	@printf '  quartermaster enable <name>\n\n'
	@printf 'Then start the daemon:\n'
	@printf '  quartermaster &\n\n'

uninstall:
	rm -f $(DESTDIR)$(SBINDIR)/quartermaster
	rm -f $(DESTDIR)$(SBINDIR)/quartermaster-notify
	rm -rf $(DESTDIR)$(SHAREDIR)
	@printf '\nBinaries and shipped rules removed.\n'
	@printf 'Left in place: $(CONFDIR) and any enabled rules in /etc/udev/rules.d/\n'
	@printf 'Remove those by hand if you want them gone.\n\n'

clean:
	rm -f quartermaster quartermaster-notify

.PHONY: all install uninstall clean
