SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)
DEPS := $(OBJS:obj/%.o=obj/%.d)

all: lexer
	./lexer

enginetest: obj/libengine.a obj/enginetest.o
	$(CC) -o $@ $^
	@#valgrind --dsymutil=yes --leak-check=full ./enginetest enginetests
	./enginetest enginetests

lexer: obj/libengine.a obj/lexer.o
	$(CC) -o $@ $^

obj/libengine.a: obj/tokeniser.o obj/nfa.o obj/engine.o
	$(AR) rcs $@ $^

-include $(DEPS)

obj/%.o: %.c
	$(CC) -Wall -g -c -o $@ -MMD $<

clean:
	-rm test $(OBJS) $(DEPS)
