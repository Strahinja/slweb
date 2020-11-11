BINDIR=/usr/local/bin
MANDIR=/usr/local/share/man/man1
BROWSER=surf
EXAMPLE_PAGES=$(addsuffix /index.slw,examples)
EXAMPLE_PAGES_HTML=$(EXAMPLE_PAGES:.slw=.html)

%.html: %.slw
	./slweb $^ > $@

%.gz: %
	gzip -kf $<

%.ps: %.1
	groff -mandoc -t -T ps $< > $@

%.pdf: %.1
	groff -mandoc -t -T pdf $< > $@

all: examples doc

examples: ${EXAMPLE_PAGES_HTML}

view: examples
	$(BROWSER) examples/index.html

doc: slweb.pdf slweb.1.gz

install: all
	mkdir -p $(BINDIR) $(MANDIR)
	cp slweb $(BINDIR)
	cp slweb.1.gz $(MANDIR)

uninstall:
	rm $(BINDIR)/slweb $(MANDIR)/slweb.1.gz 2>/dev/null

clean: slweb.1.gz slweb.pdf $(EXAMPLE_PAGES_HTML)
	rm $^ 2>/dev/null

.PHONY: examples view doc install uninstall clean

