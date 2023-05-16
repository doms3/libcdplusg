WGET ?= wget -q
MKDIR ?= mkdir -p

CFLAGS += $(USER_CFLAGS) -std=gnu2x -O3 -Iinclude -Iext -g -MD -MP -Wall -Wextra -pthread
LDLIBS += -lxcb -lxcb-image -lportaudio -lasound -ljack -lm -lpthread

.PHONY: all clean

all : libcdplusg.a xcb-test

libcdplusg.a : src/cdplusg.o 
	$(AR) $(ARFLAGS) $@ $^

xcb-test : ext/minimp3_ex.h examples/xcb_test.o libcdplusg.a examples/backends/portaudio.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

ext/minimp3_ex.h : ext/minimp3.h
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3_ex.h -O $@

ext/minimp3.h :
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h -O $@

clean :
	$(RM) libcdplusg.a src/cdplusg.o examples/backends/portaudio.o examples/xcb_test.o xcb-test src/cdplusg.d examples/backends/portaudio.d examples/xcb_test.d

-include src/cdplusg.d examples/xcb_test.d examples/backends/portaudio.d
