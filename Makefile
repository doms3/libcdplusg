CFLAGS += $(USER_CFLAGS) -std=gnu2x -O3 -I. -Iext -g -MD -MP -Wall -Wextra -march=native -pthread
LDLIBS += -lxcb -lxcb-image -lportaudio -lasound -ljack -lm -lpthread

.PHONY: all clean

all : libcdplusg.a xcb-test

libcdplusg.a : cdplusg.o
	$(AR) $(ARFLAGS) $@ $^

xcb-test : xcb_test.o libcdplusg.a 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean :
	$(RM) libcdplusg.a cdplusg.o xcb_test.o xcb-test cdplusg.d xcb_test.d

-include cdplusg.d xcb_test.d
