all: upols_convolution

CC = gcc
# This is if you're running pipewire, you may need to tweak the directory to run on you distro
# CFLAGS = -O3 -L/usr/lib64/pipewire-0.3/jack

# If you're running jack nativaly and haven't switched to pipewire (pipewire is not recommended
# to run effects stuff since it has a way worse real time perfomance than native JACK)
# you can just use this:
CFLAGS = -03

LDLIBS = -ljack -lsndfile -lm -lfftw3
   
rt_convolution: rt_convolution.c
        ${CC} ${CFLAGS} rt_convolution.c      -o rt_convolution      ${LDLIBS}
