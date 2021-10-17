CXX = gcc
CPPFLAGS = -O3 -L/usr/lib64/pipewire-0.3/jack
CXXFLAGS = -ljack -lm -lstdc++

# more delays will be added here
all: space

space: space.cpp
        $(CXX) $(CPPFLAGS) space.cpp -o space $(CXXFLAGS)
