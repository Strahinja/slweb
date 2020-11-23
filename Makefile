SLWVERSION = v0.2.2-beta
SLWDATE = 23 Nov 2020
BINDIR = /usr/local/bin
MANDIR = /usr/local/share/man/man1
BROWSER = surf
EXAMPLE_PAGES = $(addsuffix /index.slw,examples/basic examples/includes)
#EXAMPLE_PAGES += $(addprefix examples/includes,
EXAMPLE_PAGES_HTML = $(EXAMPLE_PAGES:.slw=.html)

%.html: %.slw
	./slweb	$^	> $@

%.gz: %
	gzip	-kf $<

%.ps: %.1
	groff	-mandoc -t -T ps $<	> $@

%.pdf: %.1
	groff	-mandoc -t -T pdf $<	> $@

%: %.in
	cat	$< | sed -e "s/%VERSION%/${SLWVERSION}/g" \
		-e "s/%DATE%/${SLWDATE}/g"	> $@

all: in examples doc github-io

in: slweb slweb.1

slweb: slweb.in
	cat	$< | sed -e "s/%VERSION%/${SLWVERSION}/g" \
		-e "s/%DATE%/${SLWDATE}/g"	> $@
	chmod	0755 slweb

examples: ${EXAMPLE_PAGES_HTML}

view: examples
	$(BROWSER)	examples/basic/index.html

doc: slweb.pdf slweb.1.gz

github-io: index.html

install: all
	mkdir	-p $(BINDIR) $(MANDIR)
	cp	slweb $(BINDIR)
	chmod	0755 $(BINDIR)/slweb
	cp	slweb.1.gz $(MANDIR)

uninstall:
	rm	-f $(BINDIR)/slweb $(MANDIR)/slweb.1.gz	2>/dev/null

clean: 
	rm	-f slweb.1.gz slweb.pdf index.html $(EXAMPLE_PAGES_HTML) slweb slweb.1	2>/dev/null

.PHONY: in examples view doc install uninstall clean

