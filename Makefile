#
# Makefile
#

CC ?= gcc
CXX ?= g++
STRIP ?= strip

PREFIX = /usr/local

INCLUDEDIRS  = -I ./
INCLUDEDIRS  = -I /usr/include/libftdi1

CFLAGS += -Wall
CFLAGS += -std=c++11
CFLAGS += -O2
CFLAGS += $(INCLUDEDIRS)
#define NDEBUG to disable assert()
CFLAGS += -DNDEBUG

CXXFLAGS += $(CFLAGS)

LIBDIRS  = -L.
#LIBDIRS += -L$(STARGE_LIBRARY)

LIBS  =
LIBS += -ldl
LIBS += -lftdi1
LIBS += -lusb-1.0
LIBS += -lm -lrt -lpthread

OBJS = ftdiflash.o ftdispi.o

all: ftdiflash

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

ftdiflash: $(OBJS)
	$(CXX) -o $@ $^ $(LIBDIRS) $(LIBS)
	$(STRIP) ftdiflash

.PHONY: install
install: ftdiflash
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)/bin/ftdiflash

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/ftdiflash

clean:
	rm -rf *.o
	rm -rf *.d
	rm -rf ftdiflash
