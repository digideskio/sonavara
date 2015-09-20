SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)
DEPS := $(OBJS:obj/%.o=obj/%.d)

all: test
	valgrind --dsymutil=yes --leak-check=full ./test

test: obj/libengine.a obj/test.o
	$(CC) -o $@ $^

obj/libengine.a: obj/tokeniser.o obj/nfa.o obj/engine.o
	$(AR) rcs $@ $^

-include $(DEPS)

obj/%.o: %.c
	$(CC) -g -c -o $@ -MMD $<

clean:
	-rm test $(OBJS) $(DEPS)
