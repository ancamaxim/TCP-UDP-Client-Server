SOURCES=server.cpp subscriber.cpp
LIBRARY=nope
INCPATHS=include
LIBPATHS=.
LDFLAGS=
CFLAGS=-c -Wall -Werror -Wno-error=unused-variable
CC=gcc
CPPFLAGS=$(CFLAGS) -std=c++17
CPP=g++

OBJECTS=$(SOURCES:.cpp=.o)

INCFLAGS=$(foreach TMP,$(INCPATHS),-I$(TMP))
LIBFLAGS=$(foreach TMP,$(LIBPATHS),-L$(TMP))

all: $(SOURCES)

server: server.o
		$(CPP) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

subcriber: subscriber.o
		$(CPP) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp
		$(CPP) $(INCFLAGS) $(CPPFLAGS) -fpic -c $< -o $@

clean:
		rm -rf $(OBJECTS) $(BINARY) server subscriber
