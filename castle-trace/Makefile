PROGNAME= castle-trace 
SRC = castle_trace.c

CFLAGS          += -Werror
CFLAGS          += -Wall
CFLAGS          += -Wno-unused
CFLAGS          += -D_GNU_SOURCE
#CFLAGS          += -fPIC
CFLAGS          += -g
CFLAGS 			+= -lcastle

$(PROGNAME): $(SRC)
	$(CC) $(CFLAGS) -o $(PROGNAME) $(SRC)

all: $(PROGNAME)

install: all
	install castle-trace $(BUILD_ROOT)/usr/sbin

clean:
	rm -f $(PROGNAME)
