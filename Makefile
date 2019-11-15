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
jpeg2pdf: Jpeg2PDF.o testMain.o $(SRCS)/Jpeg2PDF.h
	$(CC) $^ -o $@

Jpeg2PDF.o: $(SRCS)/Jpeg2PDF.c $(SRCS)/Jpeg2PDF.h
	$(CC) $(CFLAGS) $^

testMain.o: $(SRCS)/testMain.c
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
