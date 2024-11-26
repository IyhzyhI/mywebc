source = $(wildcard *.c)
mysource = $(patsubst %.c,%.o,$(source))

all:client

client:$(mysource)
	gcc -g -o $@ $^ -I ../include -l event

%.o:%.c
	gcc -c -I ../include -o $@ $<

clean:
	-@rm -rf *.o
	-@rm -rf client

.PHONY:clean all

