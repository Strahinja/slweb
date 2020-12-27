redo-ifchange all
BINDIR=/usr/local/bin
MANDIR=/usr/local/share/man/man1
install -d $BINDIR $MANDIR
install -m 0755 slweb $BINDIR
install -m 0644 slweb.1.gz $MANDIR

