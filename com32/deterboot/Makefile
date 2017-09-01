## -----------------------------------------------------------------------
##
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Deterboot module
##

VPATH = $(SRC)
include $(MAKEDIR)/elf.mk

#LNXLIBS	   = $(objdir)/com32/libutil/libutil_lnx.a

MODULES	  = deterboot.c32 deterboot-test.c32 edgeboot.c32
TESTFILES =
CFLAGS += -I$(topdir)/com32
CFLAGS += -I$(topdir)/core
CFLAGS += -I$(topdir)/core/lwip/src/include
CFLAGS += -I$(topdir)/core/lwip/src/include/ipv4

OBJS = deterboot.o main.o

TOBJS = deterboot-test.o testing.o deterboot.o

all: $(MODULES) $(TESTFILES)

deterboot.elf : $(OBJS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

deterboot-test.elf : $(TOBJS) $(C_LIBS) $(LIBHIREDIS)
	$(LD) $(LDFLAGS) -o $@ $^

edgeboot.elf : edgeboot.o deterboot.o $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com
	rm -f *~ \#*

install:

-include .*.d
