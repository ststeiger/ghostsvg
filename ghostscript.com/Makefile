# GNU makefile for generating static pages from wiki source.

PAGES = $(wildcard *.awki)
HTML = $(PAGES:.awki=.html)
SCRIPT = parser.awk

all: $(HTML)

%.html: %.awki $(SCRIPT)
	awk -f parser.awk $< > $@

clean:
	$(RM) $(HTML)

.PHONEY: all clean
