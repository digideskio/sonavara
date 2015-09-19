SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)

engine: $(OBJS)
	$(CC) -o $@ $^

obj/%.o: %.c
	$(CC) -g -c -o $@ $^

clean:
	-rm engine $(OBJS)
