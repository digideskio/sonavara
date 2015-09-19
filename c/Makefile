SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)

all: test
	valgrind --dsymutil=yes --leak-check=full ./test

test: obj/libengine.a obj/test.o
	$(CC) -o $@ $^

obj/libengine.a: obj/engine.o
	$(AR) rcs $@ $^

obj/%.o: %.c
	$(CC) -g -c -o $@ $^

clean:
	-rm engine $(OBJS)
