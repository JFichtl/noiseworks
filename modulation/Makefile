CXX = gcc
# take out the second flag if you're using native JACK
CPPFLAGS = -O3 -L/usr/lib64/pipewire-0.3/jack
CXXFLAGS = -ljack -lm -lstdc++

all: flutter_master_flanger

flanger: flanger.cpp
	$(CXX) $(CPPFLAGS) flutter_master_flanger.cpp -o flutter_master_flanger  $(CXXFLAGS)
