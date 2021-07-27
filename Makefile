# Makefile for hdparm

# DESTDIR is for non root installs (eg packages, NFS) only!
DESTDIR =

binprefix = 
manprefix = /usr
exec_prefix = $(binprefix)/
sbindir = $(exec_prefix)sbin
mandir = $(manprefix)/share/man
oldmandir = $(manprefix)/man

ifndef CC
CC = gcc
endif
CFLAGS := -O2 -W -Wall -Wbad-function-cast -Wcast-align -Wpointer-arith -Wcast-qual -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -fkeep-inline-functions -Wwrite-strings -Waggregate-return -Wnested-externs -Wtrigraphs $(CFLAGS)


LDFLAGS = -s
INSTALL = install
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_DIR = $(INSTALL) -m 755 -d
INSTALL_PROGRAM = $(INSTALL)

all: hdparm

hdparm: hdparm.o identify.o hdparm.h
	$(CC) $(LDFLAGS) -o hdparm hdparm.o identify.o
 
install: all hdparm.8
	if [ ! -z $(DESTDIR) ]; then $(INSTALL_DIR) $(DESTDIR) ; fi
	if [ ! -z $(DESTDIR)$(sbindir) ]; then $(INSTALL_DIR) $(DESTDIR)$(sbindir) ; fi
	if [ ! -z $(DESTDIR)$(mandir) ]; then $(INSTALL_DIR) $(DESTDIR)$(mandir) ; fi
	if [ ! -z $(DESTDIR)$(mandir)/man8/ ]; then $(INSTALL_DIR) $(DESTDIR)$(mandir)/man8/ ; fi
	if [ -f $(DESTDIR)$(sbindir)/hdparm ]; then rm -f $(DESTDIR)$(sbindir)/hdparm ; fi
	if [ -f $(DESTDIR)$(mandir)/man8/hdparm.8 ]; then rm -f $(DESTDIR)$(mandir)/man8/hdparm.8 ;\
	elif [ -f $(DESTDIR)$(oldmandir)/man8/hdparm.8 ]; then rm -f $(DESTDIR)$(oldmandir)/man8/hdparm.8 ; fi
	$(INSTALL_PROGRAM) -D hdparm $(DESTDIR)$(sbindir)/hdparm
	if [ -d $(DESTDIR)$(mandir) ]; then $(INSTALL_DATA) -D hdparm.8 $(DESTDIR)$(mandir)/man8/hdparm.8 ;\
	elif [ -d $(DESTDIR)$(oldmandir) ]; then $(INSTALL_DATA) -D hdparm.8 $(DESTDIR)$(oldmandir)/man8/hdparm.8 ; fi

clean:
	rm -f hdparm *.o core

