SOURCE_FILES     := $(wildcard *.c)
HEADER_FILES     := $(wildcard *.h)

##
## Build options
##

CFLAGS          += -Werror
CFLAGS          += -Wall
CFLAGS          += -Wno-unused
CFLAGS          += -D_GNU_SOURCE
#CFLAGS          += -fPIC
CFLAGS          += -g

MAIN_OBJS  = castle_probe_device

##
## "all" Target
##

all: castle_probe_device mkcastlefs

##
## Other targets
##

castle_probe_device: castle_probe_device.o
	gcc $(CFLAGS) castle_probe_device.o -o $@

mkcastlefs: mkcastlefs.o

%.o: %.c $(HEADER_FILES)
	gcc $(CFLAGS) -c -o $*.o $*.c

tags: $(SOURCE_FILES) $(HEADER_FILES)
	ctags $(SOURCE_FILES) $(HEADER_FILES)

.PHONY: clean
clean:
	rm -f *.o castle_probe_device mkcastlefs

.PHONY: install
install: castle_probe_device mkcastlefs
	install -D castle_probe_device $(DESTDIR)/usr/sbin/castle_probe_device
	install -D castle $(DESTDIR)/etc/rc.d/init.d/castle
	install -D castle_claim_empty $(DESTDIR)/etc/rc.d/init.d/castle_claim_empty
	install -D init-utils $(DESTDIR)/usr/share/castle-fs/init-utils
	install -D castle-fs-init.sh $(DESTDIR)/usr/share/castle-fs/castle-fs-init
	install -D castle-fs-fini.sh $(DESTDIR)/usr/share/castle-fs/castle-fs-fini
	install -D castle-smart-weigh-drive $(DESTDIR)/usr/sbin/castle-smart-weigh-drive
	install -D castle-smart-spank-drive $(DESTDIR)/usr/sbin/castle-smart-spank-drive
	install -D castle-smart-process-drive $(DESTDIR)/usr/sbin/castle-smart-process-drive
	install -D castle-find-collection $(DESTDIR)/usr/sbin/castle-find-collection
	install -D castle-scan $(DESTDIR)/usr/sbin/castle-scan
	install -D castle-claim-empty $(DESTDIR)/usr/sbin/castle-claim-empty
	install -D castle-create $(DESTDIR)/usr/sbin/castle-create
	install -D check-ssd $(DESTDIR)/usr/sbin/check-ssd
	install -D mkcastlefs $(DESTDIR)/sbin/mkcastlefs
	install -D castle_probe_device $(DESTDIR)/usr/sbin/castle-probe-device
	install -d $(DESTDIR)/var/lib/castle-fs -m 0755 -o root
