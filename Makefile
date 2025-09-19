PROGRAM := fanController
BINDIR ?= /opt
SYSTEMDIR ?= /usr/lib/systemd/system
DEBUG ?= 0
DESTDIR ?=

CFLAGS ?= -Wall -g
LDFLAGS ?=

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG
endif

.PHONY: all install uninstall clean

all: $(PROGRAM)-bin

$(PROGRAM)-bin: $(PROGRAM).o
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(PROGRAM).o -lnvidia-ml

$(PROGRAM).o: $(PROGRAM).c
	$(CC) $(CFLAGS) -c $(PROGRAM).c

install: $(PROGRAM)-bin
	install -Dm755 $(PROGRAM) $(DESTDIR)$(BINDIR)/$(PROGRAM)
	install -Dm644 nvidia-fancontroller.service $(DESTDIR)$(SYSTEMDIR)/nvidia-fancontroller.service
	-systemctl daemon-reload
	-systemctl enable --now nvidia-fancontroller

uninstall:
	-systemctl disable --now nvidia-fancontroller
	--rm -f $(DESTDIR)$(BINDIR)/$(PROGRAM)
	--rm -f $(DESTDIR)$(SYSTEMDIR)/nvidia-fancontroller.service
	-systemctl daemon-reload
	$(MAKE) clean

clean:
	$(RM) $(PROGRAM) $(PROGRAM).o
