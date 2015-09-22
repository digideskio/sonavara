SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)
DEPS := $(OBJS:obj/%.o=obj/%.d)

all: enginetest
	@#valgrind --dsymutil=yes --leak-check=full ./enginetest enginetests
	./enginetest enginetests

enginetest: obj/engine.o obj/enginetest.o
	$(CC) -o $@ $^

obj/enginetest.o: enginetest.c
	$(CC) -Wall -g -c -o $@ $^

obj/engine.o: tokeniser.c nfa.c engine.c
	cat $^ | $(CC) -Wall -g -c -o $@ -x c -

clean:
	-rm enginetest $(OBJS) $(DEPS)
