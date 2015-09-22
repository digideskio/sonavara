SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=obj/%.o)
DEPS := $(OBJS:obj/%.o=obj/%.d)

all: enginetest
	@#valgrind --dsymutil=yes --leak-check=full ./enginetest enginetests
	./enginetest enginetests

enginetest: obj/enginetest.o
	$(CC) -o $@ $^

-include $(DEPS)

obj/%.o: %.c
	$(CC) -Wall -g -c -o $@ -MMD $<

clean:
	-rm enginetest $(OBJS) $(DEPS)
