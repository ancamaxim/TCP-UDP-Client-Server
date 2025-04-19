CPP=g++
CPPFLAGS=-Wall -Werror -Wno-error=unused-variable -std=c++23

all: server subscriber

utils.o: utils.cpp
	$(CPP) $(CPPFLAGS) -c utils.cpp -o utils.o

server: server.cpp utils.o
	$(CPP) $(CPPFLAGS) server.cpp utils.o -o $@

subscriber: subscriber.cpp utils.o
	$(CPP) $(CPPFLAGS) subscriber.cpp utils.o -o $@

clean:
	rm -rf utils.o server subscriber
