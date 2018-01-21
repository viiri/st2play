CC = gcc
CFLAGS = -Wall -O3 -I/usr/include/SDL2
LD = gcc
LDFLAGS =
LIBS = -lSDL
OBJS = stmload.o st2play.o stmod.o

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: st2play

st2play: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
