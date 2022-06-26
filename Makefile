CFLAGS += -I. -MD -MP
LDLIBS += -lxcb -lxcb-image

.PHONY: all clean

all : libcdplusg.a xcb_test

libcdplusg.a : cdplusg.o
	$(AR) $(ARFLAGS) $@ $^

xcb_test : xcb_test.o libcdplusg.a 

clean :
	$(RM) libcdplusg.a cdplusg.o xcb_test.o xcb_test cdplusg.d xcb_test.d

-include cdplusg.d xcb_test.d
