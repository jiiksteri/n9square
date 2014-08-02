PROG := n9square
OBJS := checkin.o
TEST_OBJS := $(patsubst %.c,%.o,$(wildcard test/suite_*.c))

CFLAGS += -Wall -g $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-3.0 libsoup-2.4 json-glib-1.0)
LDFLAGS += $(shell pkg-config --libs gtk+-3.0 webkit2gtk-3.0 libsoup-2.4 json-glib-1.0)
LDFLAGS_TEST := $(LDFLAGS) -lcunit

.PHONY: all clean test

all: $(PROG)

clean:
	$(RM) $(PROG) $(OBJS) $(TEST_OBJS) *.d test/*.d

test: n9square test/run
	test/run $(TEST_OBJS)

test/run: $(OBJS) $(TEST_OBJS) test/run.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_TEST) -o $@ $(OBJS) $(TEST_OBJS) test/run.o

$(PROG): $(PROG).o $(OBJS)

-include $(OBJS:.o=.d)

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
		sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@$(RM) $*.d.tmp
