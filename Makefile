CFLAGS += -I.
LDFLAGS += -lxcb -lxcb-image

.PHONY: all clean

all : libcdplusg.a xcb_test

libcdplusg.a : cdplusg.o
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

xcb_test : xcb_test.o libcdplusg.a 
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

clean :
	$(RM) libcdplusg.a cdplusg.o xcb_test.o xcb_test
