source = $(wildcard *.c)
myo = $(patsubst %.c,%.o,$(source))

all:client

client:$(myo)
	gcc -g -I ../include -o $@ $(myo)

%.o:%.c
	gcc -c -I ../include -o $@ $<

clean:
	-@rm -rf server
	-@rm -rf *.o

.PHONY:clean all
