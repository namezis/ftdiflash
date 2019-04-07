#
# Makefile
#

CC ?= gcc
CXX ?= g++
STRIP ?= strip

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

clean:
	rm -rf *.o
	rm -rf *.d
	rm -rf ftdiflash
