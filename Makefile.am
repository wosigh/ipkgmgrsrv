VERSION = @VERSION@

LIBIPKG = @LIBIPKG@

CC = @CC@
CFLAGS = @CFLAGS@ -DVERSION=\"$(VERSION)\"
LDFLAGS = @LDFLAGS@

all: clean @PACKAGE@

@PACKAGE@: src/main.c
	$(CC) $(CFLAGS) $(LDFLAGS) src/main.c src/ipkgmgr.c src/utils.c $(LIBIPKG) -o @PACKAGE@

clean:
	rm -f @PACKAGE@

distclean:
	rm -rf @PACKAGE@ config.* Makefile aclocal* configure autom4te* install-sh missing Makefile.in
