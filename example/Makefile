CXX = g++
CXXFLAGS = -Wall -g
LDFLAGS = -lmymuduo -lpthread
SRCS = $(wildcard *.cc)
TARGETS = $(SRCS:.cc=)

all: $(TARGETS)

%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(TARGETS)

.PHONY: all clean
