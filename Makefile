
INCLUDE=./include
SRC=./src
CC=clang++
CCFLAGS=-std=c++1z -fdelayed-template-parsing -fms-compatibility -g -I $(INCLUDE)
LIBS=-lstdc++fs

SOURCES:=$(shell find $(SRC) -name '*.cpp')
OBJECTS=$(SOURCES:.cpp=.o)

all: app

%.o: %.cpp
	$(CC) $(CCFLAGS) -c $< -o $@

app: $(OBJECTS)
	$(CC) $(CCFLAGS) -c $^ -o $@ $(LIBS)

.PHONY: clean
clean:
	rm -f $(OBJECTS)
