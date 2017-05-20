CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=304410695_104473226
CLASSES= TCPheader.cpp

all: server client

server: $(CLASSES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

client: $(CLASSES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . --exclude=./*.txt --exclude=./cs118_proj2_sample_client_server --exclude=./upload_test --exclude=./*.tar.gz && mv /tmp/$(USERID).tar.gz .

