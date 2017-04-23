# (c)2014 ThingWorx
# C SDK Example App Make file

ifeq ($(TW_SDK_ROOT),)
TW_SDK_ROOT = ../../..
endif

################## APP SPECIFIC DEFINES #######################

# TW_APP_NAME is the desired name of the application executable
TW_APP_NAME      = SteamSensorWithFileTransferAndTunneling

# TW_APP_DIR is where your application directory is
TW_APP_DIR       := ..

# TW_APP_INC_DIR is where your application source files are located
TW_APP_INC_DIR      := $(TW_APP_DIR)/src

# TW_APP_SRC_DIR is where your application source files are located
TW_APP_SRC_DIR       := $(TW_APP_DIR)/src

# TW_APP_SRC is a list of your source files
TW_APP_SRC     := main.c

# Extra Preprocessor defines
TW_APP_DEFS      =

# Dependencies on external libraries
LIB_DEPS	 = -lm

include ./Make.CommonSettings

#####################################################
# Should not need to Modify anything below this line
#####################################################
#Set up the application config settings
include $(TW_SDK_ROOT)/build/Make.settings

TW_SDK_LIB_NAME = Tw
TW_SDK_LIB_PATH = $(TW_SDK_ROOT)/build/obj/$(PLATFORM)/$(BUILDDIR)

#SDK Include paths
SDK_INC_DIRS := $(patsubst %,-I %,$(shell find $(TW_SDK_ROOT)/src -maxdepth 1 -type d)) -I $(TW_SDK_ROOT)/src/thirdParty/cJSON

# Default to an x86 build
ifeq ($(PLATFORM),)
PLATFORM          = gcc-linux-x86-64
endif
$(warning )
$(warning ############ Building with PLATFORM=$(PLATFORM) ################)
$(warning )
# We don't build for Windows in Make
ifeq ($(PLATFORM),win32)
$(error ############ Please use Visual Studio to build for Windows ################)
exit 1
endif

# Set our build type (debug or release), default is debug
ifeq ($(BUILD),release)
DDEBUG          = -Os -s -fno-asynchronous-unwind-tables -Wl,--strip-all
BUILDDIR        = release
else
DDEBUG          = -D"DEBUG"  -g -O0 
BUILDDIR        = debug
endif

#Include the platform specific compiler settings
include $(TW_SDK_ROOT)/build/platforms/$(PLATFORM)/Makefile.$(PLATFORM)

.PHONY: all

all: app

sdk: $(TW_SDK_LIB_NAME) 

lib: obj $(LIB_OBJS)

$(TW_SDK_LIB_NAME): $(TW_APP_DIR)/linux/Make.CommonSettings
	cd $(TW_SDK_ROOT)/build; make -f Makefile PLATFORM=$(PLATFORM) BUILD=$(BUILDDIR) 

obj:
	if [ ! -d obj ] ; then mkdir obj ; mkdir obj/$(BUILDDIR) ; fi


clean:
	rm -Rf obj/$(PLATFORM)/$(BUILDDIR)
	rm -Rf bin/$(PLATFORM)/$(BUILDDIR)
	rm -f core *.core core.*

#
# compilation
#
vpath %.h .:$(INCDIR)

$(LIB_OBJS): 
#	$(ECHO) LIB_SRCS = $(TW_SDK_SRC) 
	$(CC) $(CC_OPTS) $(DEFS) -c $(subst obj/$(BUILDDIR)/,,$(subst .o,.c,$@)) 
	mkdir -p $(LIBDEST); mv $(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) $(LIBDEST)/$(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) 

app: $(TW_SDK_LIB_NAME)
	$(CC) $(CC_OPTS) $(DDEBUG) -c -I $(TW_APP_INC_DIR) $(SDK_INC_DIRS) $(DEFS) $(patsubst %,$(TW_APP_SRC_DIR)/%,$(TW_APP_SRC)) 
	mkdir -p obj/$(PLATFORM)/$(BUILDDIR); mv *.o  obj/$(PLATFORM)/$(BUILDDIR)
	$(LINKER) $(LIBOPTS) -L$(LIBDIR) -L$(TW_SDK_LIB_PATH) $(patsubst %.c,obj/$(PLATFORM)/$(BUILDDIR)/%.o,$(notdir $(TW_APP_SRC)))  $(STATIC_LIBS) -l$(TW_SDK_LIB_NAME) $(LIB_DEPS) 
	mkdir -p bin/$(PLATFORM)/$(BUILDDIR) ; mv a.out bin/$(PLATFORM)/$(BUILDDIR)/$(TW_APP_NAME); rm -f $(subst .c,.o,$(TW_APP_SRC))
