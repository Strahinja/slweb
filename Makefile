SLWVERSION = v0.3.0
SLWDATE = 10 Dec 2020
BINDIR = /usr/local/bin
MANDIR = /usr/local/share/man/man1
CFLAGS=-g
LIBS=-lunistring
BROWSER = surf
EXAMPLE_PAGES = $(addsuffix /index.slw,examples/basic examples/includes examples/tags)
#EXAMPLE_PAGES += $(addprefix examples/includes,
EXAMPLE_PAGES_HTML = $(EXAMPLE_PAGES:.slw=.html)

%.html: %.slw
	./slweb	$<	> $@

%.gz: %
	gzip	-kf $<

%.ps: %.1
	groff	-mandoc -t -T ps $<	> $@

%.pdf: %.1
	groff	-mandoc -t -T pdf $<	> $@

%: %.in
	cat	$< | sed -e "s/%VERSION%/${SLWVERSION}/g" \
		-e "s/%DATE%/${SLWDATE}/g"	> $@

all: in examples doc index.html

in: slweb slweb.1

index.html: slweb index.slw

slweb: slweb.o
	$(CC)	$(CFLAGS) $(LIBS) -o $@ $<

examples: ${EXAMPLE_PAGES_HTML} slweb

view: index.html
	$(BROWSER)	index.html

doc: slweb.pdf slweb.1.gz

install: all
	mkdir	-p $(BINDIR) $(MANDIR)
	cp	slweb $(BINDIR)
	chmod	0755 $(BINDIR)/slweb
	cp	slweb.1.gz $(MANDIR)

uninstall:
	rm	-f $(BINDIR)/slweb $(MANDIR)/slweb.1.gz	2>/dev/null

clean: 
	rm	-f *.o slweb.1.gz slweb.pdf index.html $(EXAMPLE_PAGES_HTML) slweb slweb.1	2>/dev/null

rebuild: clean all

.PHONY: in examples view doc install uninstall clean rebuild

