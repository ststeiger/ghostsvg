# GNU makefile for generating static pages from wiki source.

PAGES   := $(wildcard *.awki)
SPECIAL := contents.html
SCRIPT  := awkihtml.awk

HTML    := $(PAGES:.awki=.html)

all: $(HTML) $(SPECIAL)

%.html: %.awki $(SCRIPT)
	awk -f $(SCRIPT) $< > $@

contents.awki: $(SCRIPT) listawki.awk
	ls *.awki | awk -f listawki.awk -v head="Available pages:" > $@

clean:
	$(RM) $(HTML)
	$(RM) $(SPECIAL) $(SPECIAL:.html=.awki)

.PHONEY: all clean
