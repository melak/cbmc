SRC = casting_replace_symbol.cpp \
      linking.cpp \
      linking_diagnostics.cpp \
      remove_internal_symbols.cpp \
      static_lifetime_init.cpp \
      # Empty last line

INCLUDES= -I ..

include ../config.inc
include ../common

CLEANFILES = linking$(LIBEXT)

all: linking$(LIBEXT)

###############################################################################

linking$(LIBEXT): $(OBJ)
	$(LINKLIB)
