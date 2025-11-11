CXX=g++
CXXFLAGS=-Wall -g -O2
CXXFLAGS+=-std=c++17
EXTRA_CFLAGS?=-fsanitize=address -fno-omit-frame-pointer
LDFLAGS=-pthread
SRC=06factory.cpp

all: factory

factory: $(SRC)
	$(CXX) $(CXXFLAGS) $(EXTRA_CFLAGS) -o factory $(SRC) $(LDFLAGS) $(EXTRA_CFLAGS)

clean:
	rm -f factory