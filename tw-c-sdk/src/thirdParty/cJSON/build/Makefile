#
# ThingWorx (c) 2013
# Build for an edge module
#
######################################

###### MODULE SPECIFIC DEFINES #######
MODULE_NAME     = cJSON
MOVE_HDRS       = 
APP_SRC         = 
APP_NAME        = 
# Dependencies on other modules & external libraries
MOD_DEPS        = 
EXT_LIB_DEPS	= 
# Any additonal C++ src files not in the src dir
EXTRA_SRC       = 
EXTRA_SRC_DIR   = 
EXTRA_INCDIRS   = 
# Extra C libraries
CLIB_SRC_DIR    = ..
CLIB_INC_DIR    = .. 
CLIB_SRCS       = cJSON.c
CLIB_OBJS       = $(patsubst %.c,obj/$(PLATFORM)/$(BUILDDIR)/%.o,$(CLIB_SRCS))
# Extra Preprocessor defines
MOD_DEFS        = PALANTIRI
######################################
# Default to a linux build
ifeq ($(PLATFORM),)
PLATFORM          = linux-x86
endif

PS_ROOT         = ../../..

include $(PS_ROOT)/buildtools/$(PLATFORM)/makefile.$(PLATFORM)
include $(PS_ROOT)/buildtools/makefile.common


