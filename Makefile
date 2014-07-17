PROG := n9square
OBJS := n9square.o

CFLAGS += -Wall -g $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-3.0 libsoup-2.4 json-glib-1.0)
LDFLAGS += $(shell pkg-config --libs gtk+-3.0 webkit2gtk-3.0 libsoup-2.4 json-glib-1.0)

.PHONY: all clean

all: $(PROG)

clean:
	$(RM) $(PROG) $(OBJS) *.d

$(PROG): $(OBJS)


%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
		sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@$(RM) $*.d.tmp
