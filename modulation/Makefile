CXX = gcc
# take out the second flag if you're using native JACK
CPPFLAGS = -O3 -L/usr/lib64/pipewire-0.3/jack
CXXFLAGS = -ljack -lm -lstdc++

all: flanger

flanger: flanger.cpp
	$(CXX) $(CPPFLAGS) flanger.cpp -o flanger  $(CXXFLAGS)
