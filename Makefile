# $Id$
#
# include Husky-Makefile-Config
ifeq ($(DEBIAN), 1)
# Every Debian-Source-Paket has one included.
include /usr/share/husky/huskymak.cfg
else
include ../huskymak.cfg
endif

OBJS    = htick$(OBJ) global$(OBJ) toss$(OBJ) fcommon$(OBJ) \
          scan$(OBJ) areafix$(OBJ) add_desc$(OBJ) seenby$(OBJ) \
	  hatch$(OBJ) filelist$(OBJ) filecase$(OBJ) report$(OBJ) clean$(OBJ)
MAN1PAGE = man/htick.1
MAN1DIR  = $(MANDIR)$(DIRSEP)man1
SRC_DIR = src/

ifndef ECHO
ECHO	= echo
endif

ifeq ($(DEBUG), 1)
  CFLAGS = $(DEBCFLAGS) -Ih -I$(INCDIR) $(WARNFLAGS)
  LFLAGS = $(DEBLFLAGS)
else
  CFLAGS = $(OPTCFLAGS) -Ih -I$(INCDIR) $(WARNFLAGS)
  LFLAGS = $(OPTLFLAGS)
endif

ifneq ($(DYNLIBS), 1)
  LFLAGS += -static -lc
endif

ifeq ($(SHORTNAME), 1)
  LIBS  = -L$(LIBDIR) -lfidoconf -lsmapi
else
  LIBS  = -L$(LIBDIR) -lfidoconfig -lsmapi
endif

CDEFS=-D$(OSTYPE) $(ADDCDEFS)

all: $(OBJS) htick$(EXE)

%$(OBJ): $(SRC_DIR)%.c
	$(CC) $(CFLAGS) $(CDEFS) $(SRC_DIR)$*.c

htick$(EXE): $(OBJS)
	$(CC) $(LFLAGS) -o htick$(EXE) $(OBJS) $(LIBS)

clean:
	-$(RM) $(RMOPT) *$(OBJ)
	-$(RM) $(RMOPT) core

distclean: clean
	-$(RM) $(RMOPT) htick$(EXE)
	-$(RM) $(RMOPT) *.1.gz
	-$(RM) $(RMOPT) *.log

install: htick$(EXE)
	$(INSTALL) $(IBOPT) htick$(EXE) $(BINDIR)
	$(INSTALL) $(IMOPT) $(MAN1PAGE) $(MAN1DIR)
	$(ECHO) To install documentation: change directory to doc and run make install

uninstall:
	$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)htick$(EXE)
	$(RM) $(RMOPT) $(MAN1DIR)$(DIRSEP)$(MAN1PAGE)

