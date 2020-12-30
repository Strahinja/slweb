redo-ifchange all
PREFIX=/usr/local
BINDIR=$PREFIX/bin
DOCDIR=$PREFIX/share/doc/slweb
MANDIR=$PREFIX/share/man/man1
install -d $BINDIR $DOCDIR $MANDIR
install -m 0755 slweb $BINDIR
install -m 0644 slweb.pdf $DOCDIR
install -m 0644 slweb.1.gz $MANDIR

