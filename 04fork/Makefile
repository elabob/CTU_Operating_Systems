# nastavujem kompilatory a flags
CXX = g++                 # C++ kompilator
CC = gcc                  # C kompilator
CXXFLAGS = -Wall $(EXTRA_CFLAGS)  # warningy a volitelne extra flags pre C++
CFLAGS = -Wall $(EXTRA_CFLAGS)    # warningy a volitelne extra flags pre C

# forkpipe program
FORKPIPE = forkpipe
FORKPIPE_SRC = 04fork.cpp         # zdrojak pre forkpipe

# nsd program
NSD = nsd
NSD_SRC = nsd_src/nsd_main.c nsd_src/nsd.c nsd_src/nd.c  # zdrojaky pre nsd
NSD_HDR = nsd_src/nsd.h nsd_src/nd.h                     # header subory pre nsd

# default target - buildnem vsetko
all: $(FORKPIPE) $(NSD)

# build forkpipe
$(FORKPIPE): $(FORKPIPE_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^     # kompilujem C++ zdrojak do forkpipe

# build nsd
$(NSD): $(NSD_SRC) $(NSD_HDR)
	$(CC) $(CFLAGS) -o $@ $^        # kompilujem C zdrojaky do nsd

# cistenie - odstranenie binarnych suborov
clean:
	rm -f $(FORKPIPE) $(NSD)

# tar pre odovzdanie
tar:
	tar czf osy04.tgz Makefile 04fork.cpp nsd_src/*.c nsd_src/*.h
