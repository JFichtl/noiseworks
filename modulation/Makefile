CC       = gcc
CFLAGS   = -L/usr/lib64/pipewire-0.3/jack  -Wall
LDLIBS   = -ljack -lm -lfftw3
CXXFLAGS = -ljack -lm -lstdc++
TARGET   = fft_modulation

OBJFILE   = $(patsubst %, %.o, $(TARGET))

all: $(TARGET)

$(TARGET): $(OBJFILE)
        $(CC) $(CFLAGS) -o $(TARGET) $(OBJFILE) $(LDLIBS)
clean:
        rm -f *.o *~
