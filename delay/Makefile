CXX = gcc
CPPFLAGS = -O3 -L/usr/lib64/pipewire-0.3/jack
CXXFLAGS = -ljack -lm -lstdc++

all: jack_delay_cpp space_echo_alike flanger

space: space.cpp
        $(CXX) $(CPPFLAGS) space.cpp -o space $(CXXFLAGS)
