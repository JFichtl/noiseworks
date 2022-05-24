CC       = gcc
CFLAGS   = -L/usr/lib64/pipewire-0.3/jack  -Wall
LDLIBS   = -ljack -lm
CXXFLAGS = -ljack -lm -lstdc++
TARGET   =  schroeder_logan_reverb

OBJFILE   = $(patsubst %, %.o, $(TARGET))

all: $(TARGET)

$(TARGET): $(OBJFILE)
        $(CC) $(CFLAGS) -o $(TARGET) $(OBJFILE) $(LDLIBS)
clean:
        rm -f *.o *~
