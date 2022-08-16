WGET ?= wget -q
MKDIR ?= mkdir -p

CFLAGS += $(USER_CFLAGS) -std=gnu2x -O3 -I. -Iext -g -MD -MP -Wall -Wextra -march=native -pthread
LDLIBS += -lxcb -lxcb-image -lportaudio -lasound -ljack -lm -lpthread

.PHONY: all clean

all : libcdplusg.a xcb-test

libcdplusg.a : cdplusg.o ext/minimp3_ex.h
	$(AR) $(ARFLAGS) $@ $^

xcb-test : xcb_test.o libcdplusg.a 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

ext/minimp3_ex.h : ext/minimp3.h
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3_ex.h -O $@

ext/minimp3.h :
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h -O $@

clean :
	$(RM) libcdplusg.a cdplusg.o xcb_test.o xcb-test cdplusg.d xcb_test.d

-include cdplusg.d xcb_test.d
