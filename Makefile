WGET ?= wget -q
MKDIR ?= mkdir -p

PORTAUDIO_LIBS=$(shell pkg-config --libs portaudio-2.0)
PORTAUDIO_CFLAGS=$(shell pkg-config --cflags portaudio-2.0)

XCB_LIBS=$(shell pkg-config --libs xcb)
XCB_CFLAGS=$(shell pkg-config --cflags xcb)

XCB_IMAGE_LIBS=$(shell pkg-config --libs xcb-image)
XCB_IMAGE_CFLAGS=$(shell pkg-config --cflags xcb-image)

DEFAULT_CFLAGS = -std=c2x -pedantic -O2 -Iinclude -Iext -g -MD -MP -Wall -Wextra

CFLAGS += $(USER_CFLAGS) $(DEFAULT_CFLAGS) $(PORTAUDIO_CFLAGS) $(XCB_CFLAGS) $(XCB_IMAGE_CFLAGS)
LDLIBS += $(USER_LDLIBS) $(PORTAUDIO_LIBS) $(XCB_LIBS) $(XCB_IMAGE_LIBS)

XCB_TEST_OBJS = \
	examples/xcb_test.o \
	examples/backends/portaudio.o

.PHONY: all clean

all : libcdplusg.a xcb-test

libcdplusg.a : src/cdplusg.o 
	$(AR) $(ARFLAGS) $@ $^

xcb-test : ext/minimp3_ex.h examples/xcb_test.o examples/backends/portaudio.o libcdplusg.a
	$(CC) $(LDFLAGS) $(XCB_TEST_OBJS) libcdplusg.a $(LDLIBS) -o $@

ext/minimp3_ex.h : ext/minimp3.h
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3_ex.h -O $@

ext/minimp3.h :
	$(MKDIR) ext
	$(WGET) https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h -O $@

clean :
	$(RM) libcdplusg.a src/cdplusg.o examples/backends/portaudio.o examples/xcb_test.o xcb-test src/cdplusg.d examples/backends/portaudio.d examples/xcb_test.d

-include src/cdplusg.d examples/xcb_test.d examples/backends/portaudio.d
