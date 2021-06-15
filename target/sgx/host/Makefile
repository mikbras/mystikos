TOP=$(abspath ../../..)
include $(TOP)/defs.mak

SUBLIBDIR=$(LIBDIR)

ARCHIVE = libmysttargetsgxhost.a

SOURCES += ../../shared/waitwake.c
SOURCES += ../../shared/poll.c
SOURCES += ../../shared/mapfile.c

CFLAGS = $(DEFAULT_CFLAGS)

INCLUDES = -I$(INCDIR)

include $(TOP)/rules.mak
