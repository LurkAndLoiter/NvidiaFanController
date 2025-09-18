PROGRAM := fanController
BINDIR ?= /opt
SYSTEMDIR ?= /usr/lib/systemd/system
DEBUG ?= 0
DESTDIR ?=

CFLAGS ?= -Wall -g
LDFLAGS ?= -lnvidia-ml

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG
endif

.PHONY: all install uninstall clean

all: $(PROGRAM)-bin

$(PROGRAM)-bin: $(PROGRAM).o
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(PROGRAM).o

$(PROGRAM).o: $(PROGRAM).c
	$(CC) $(CFLAGS) -c $(PROGRAM).c

install: $(PROGRAM)-bin
	install -Dm755 $(PROGRAM) $(DESTDIR)$(BINDIR)/$(PROGRAM)
	install -Dm644 nvidia-fancontroller.service $(DESTDIR)$(SYSTEMDIR)/nvidia-fancontroller.service
	systemctl daemon-reload || true
	systemctl enable --now nvidia-fancontroller || true

uninstall:
	-systemctl disable --now nvidia-fancontroller || true
	--rm -f $(DESTDIR)$(BINDIR)/$(PROGRAM)
	--rm -f $(DESTDIR)$(SYSTEMDIR)/nvidia-fancontroller.service
	-systemctl daemon-reload || true
	$(MAKE) clean

clean:
	$(RM) $(PROGRAM) $(PROGRAM).o
