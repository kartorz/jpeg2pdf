# $Id$

DEBUG?=0
ifeq ($(DEBUG),1)
	CFLAGS=-c -Wall -g -DJPEG2PDF_DEBUG=1
else
	CFLAGS=-c -Wall -O3
endif

CC=gcc
SRCS=src
prefix=/usr/local

all: jpeg2pdf

# build
jpeg2pdf: jpeg2pdf.o jpeg2pdfcli.o $(SRCS)/jpeg2pdf.h
	$(CC) $^ -o $@

jpeg2pdf.o: $(SRCS)/jpeg2pdf.c $(SRCS)/jpeg2pdf.h
	$(CC) $(CFLAGS) $^

jpeg2pdfcli.o: $(SRCS)/jpeg2pdfcli.c
	$(CC) $(CFLAGS) $^

# test (depends on ghostscript and poppler)
test: output.pdf
	gs -q -dNODISPLAY -c quit output.pdf && pdfinfo -box -meta output.pdf

output.pdf: jpeg2pdf *.jpg
	./jpeg2pdf -o output.pdf -t TitleTest -a AuthorTest *.jpg

#install
install: jpeg2pdf
	install -m 0755 jpeg2pdf $(prefix)/bin

# cleanup
clean:
	rm -f *.o $(SRCS)/*.gch jpeg2pdf output.pdf
