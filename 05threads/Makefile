EXTRA_CFLAGS ?= -fsanitize=address -fno-omit-frame-pointer
CXXFLAGS = -Wall -g -O2 $(EXTRA_CFLAGS)
LDFLAGS = $(EXTRA_CFLAGS)

prod-cons: prod-cons.cpp
	g++ $(CXXFLAGS) prod-cons.cpp -o prod-cons -lpthread $(LDFLAGS)

clean:
	rm -f prod-cons