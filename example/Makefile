CXX = g++
CXXFLAGS = -Wall -g	
QPS_CXXFLAGS = -Wall -O3 -DNDEBUG -DQPSTESTON 
LDFLAGS = -lmymuduo -lpthread
SRCS = $(wildcard *.cc)
TARGETS = $(SRCS:.cc=)
QPS_TARGET = QPS_testserver

all: $(TARGETS)

qps: $(QPS_TARGET)

%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(QPS_TARGET): $(QPS_TARGET).cc
	$(CXX) $(QPS_CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(TARGETS)

.PHONY: all qps clean
