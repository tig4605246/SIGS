# (c)2014 ThingWorx
# C SDK Make file

#####################################################
# Should not need to Modify anything below this line
#####################################################
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


#Include the platform specific compiler settings
include ./platforms/$(PLATFORM)/Makefile.$(PLATFORM)

###### LIBRARY DIRECTORIES #######
ifeq ($(TW_SDK_ROOT),)
TW_SDK_ROOT = ..
endif

#Include and define the application config settings
include $(TW_SDK_ROOT)/build/Make.settings

# 
DIR_LIST          = api config fileTransfer messaging porting tls utils websocket tunneling subscribedProps

TW_API_SRCS       = twApi.c twProperties.c twServices.c
TW_FILE_SRCS      = twFileManager.c twFileTransferCallbacks.c
TW_MESSAGING_SRCS = twBaseTypes.c twInfoTable.c twMessages.c twMessaging.c
TW_TLS_SRCS       = twTls.c cert.c private_key.c twConnectionInfo.c
TW_UTILS_SRCS     = list.c stringUtils.c twLogger.c twTasker.c twHttpProxy.c twNtlm.c crypto_wrapper.c
TW_WEBSOCKET_SRCS = twWebsocket.c
TW_TUNNELING_SRCS = twTunnelManager.c
TW_SUBSCRIBED_SRCS = twSubscribedProps.c

# TLS FILES
ifeq ($(NO_AXTLS),)
TLS_INC_DIR    = $(TW_SDK_ROOT)/src/thirdParty/axTLS/ssl $(TW_SDK_ROOT)/src/thirdParty/axTLS/crypto \
                 $(TW_SDK_ROOT)/src/thirdParty/axTLS/config $(TW_SDK_ROOT)/src/thirdParty/axTLS
TLS_SRCS       = crypto/aes.c crypto/bigint.c crypto/crypto_misc.c crypto/hmac.c \
               crypto/sha256.c crypto/md5.c crypto/rc4.c crypto/rsa.c crypto/sha1.c \
	           ssl/asn1.c ssl/gen_cert.c ssl/loader.c ssl/os_port.c \
	           ssl/p12.c ssl/tls1.c ssl/tls1_clnt.c ssl/x509.c ssl/tls1_svr.c
TLS_SRCS      := $(patsubst %,$(TW_SDK_ROOT)/src/thirdParty/axTLS/%,$(TLS_SRCS))
TLS_DEFS      =
TLS_LIB_DIR   =
TLS_LIBS      =
AXTLS_LIB_OBJS = $(patsubst %.c,obj/$(BUILDDIR)/%.o,$(TLS_SRCS)) 
endif

#Add in the ThirdParty source files and includes
#JOYENT_INC_DIR  = joyent-http-parser
#JOYENT_SRCS     = joyent-http-parser/http_parser.c
#JOYENT_DEFS     =

CJSON_INC_DIR    = cJSON
CJSON_SRCS       = cJSON/cJSON.c
CJSON_DEFS       = 

ifeq ($(NO_TOMCRYPT),)
TOMCRYPT_INC_DIR = tomcrypt/src/headers
TOMCRYPT_SRCS    = tomcrypt/src/ciphers/des.c tomcrypt/src/hashes/md4.c tomcrypt/src/hashes/md5.c tomcrypt/src/hashes/sha1.c tomcrypt/src/misc/error_to_string.c tomcrypt/src/misc/crypt/crypt_argchk.c tomcrypt/src/misc/base64/base64_encode.c tomcrypt/src/misc/base64/base64_decode.c
TOMCRYPT_DEFS    = LTC_NO_ROLC
endif

NTLM_INC_DIR 	= ntlm
NTLM_SRCS    	= ntlm/ntlm.c
NTLM_DEFS    	= 

WC_INC_DIR    = wildcard
WC_SRCS       = wildcard/wildcard.c
WC_DEFS       = 

#Complete source and include directory lists
TW_SDK_INCDIRS	= -I $(TW_SDK_ROOT)/src \
                  $(patsubst %,-I $(TW_SDK_ROOT)/src/%,$(DIR_LIST)) $(patsubst %,-I %,$(TLS_INC_DIR)) \
                  $(patsubst %,-I $(TW_SDK_ROOT)/src/thirdParty/%,$(CJSON_INC_DIR)) \
                  $(patsubst %,-I $(TW_SDK_ROOT)/src/thirdParty/%,$(TOMCRYPT_INC_DIR)) \
                  $(patsubst %,-I $(TW_SDK_ROOT)/src/thirdParty/%,$(NTLM_INC_DIR)) \
                  $(patsubst %,-I $(TW_SDK_ROOT)/src/thirdParty/%,$(WC_INC_DIR))		  

TW_SDK_SRC     = $(patsubst %,$(TW_SDK_ROOT)/src/api/%,$(TW_API_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/messaging/%,$(TW_MESSAGING_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/websocket/%,$(TW_WEBSOCKET_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/tunneling/%,$(TW_TUNNELING_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/utils/%,$(TW_UTILS_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/fileTransfer/%,$(TW_FILE_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/subscribedProps/%,$(TW_SUBSCRIBED_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/thirdParty/%,$(CJSON_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/thirdParty/%,$(TOMCRYPT_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/thirdParty/%,$(NTLM_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/thirdParty/%,$(WC_SRCS)) \
                 $(patsubst %,$(TW_SDK_ROOT)/src/tls/%,$(TW_TLS_SRCS)) \
                 $(OS_SRCS)

# Set our build type (debug or release), default is debug
ifeq ($(BUILD),release)
DDEBUG          = -Os -s -fno-asynchronous-unwind-tables -Wl,--strip-all -Wno-pointer-sign
BUILDDIR        = release
else
DDEBUG          = -D"DEBUG" -g -O0 -Wno-pointer-sign
BUILDDIR        = debug
endif

DOXYGEN			= doxygen

######################################
TW_SDK_LIB_NAME = Tw
CLEAN_LIST     = $(DIR_LIST)

#Set up our libraries
LIB_DEPS        = $(patsubst %,-l%,$(EXT_LIB_DEPS)) 
LIB_OBJS       := $(patsubst %.c,obj/$(BUILDDIR)/%.o,$(TW_SDK_SRC)) 
LIBNAME         = lib$(TW_SDK_LIB_NAME).a
LIBDEST         = obj/$(PLATFORM)/$(BUILDDIR)
SKIPDEFFILE     = yes

#Compiler settings
SDK_CCOPTS      = $(CC_OPTS) $(DDEBUG) $(TW_SDK_INCDIRS) $(patsubst %,-I %,$(OS_INC_DIR))
APPDEST         = bin/$(PLATFORM)/$(BUILDDIR)
DEFS            = $(patsubst %, -D%, $(TW_APP_DEFS)) $(patsubst %, -D%, $(TLS_DEFS))  $(patsubst %, -D%, $(OS_DEFS)) \
                  $(patsubst %, -D%, $(JOYENT_DEFS)) $(patsubst %, -D%, $(TOMCRYPT_DEFS)) \
                  $(patsubst %, -D%, $(CJSON_DEFS)) $(patsubst %, -D%, $(WC_DEFS)) $(patsubst %, -D%, $(NTLM_DEFS)) \

#Check to see if we should output our source list
ifeq ($(SUPPRESS_SRC_LIST), true)
ECHO            = :
else
ECHO            = @echo
endif

#
# targets
#

.PHONY: all

all: lib 
test: lib 
lib: obj $(LIBNAME) 

$(LIBNAME): $(AXTLS_LIB_OBJS) $(LIB_OBJS) 
	$(AR) $@  $(patsubst %.c,obj/$(PLATFORM)/$(BUILDDIR)/%.o,$(notdir $(TW_SDK_SRC))) $(patsubst %.c,obj/$(PLATFORM)/$(BUILDDIR)/axtls/%.o,$(notdir $(TLS_SRCS))) 
	$(RANLIB) $@
	mkdir -p $(LIBDEST) ; mv $@ $(LIBDEST)

obj:
	if [ ! -d obj ] ; then mkdir -p obj/$(PLATFORM)/$(BUILDDIR) ; fi

doc:
	$(DOXYGEN) Doxyfile

clean:
	rm -Rf obj/$(PLATFORM)/$(BUILDDIR)
	rm -Rf bin/$(PLATFORM)/$(BUILDDIR)
	rm -f core *.core core.*


#
# compilation
#
vpath %.h .:$(INCDIR)

$(LIB_OBJS): 
	$(CC) $(SDK_CCOPTS) $(DEFS) -c $(subst obj/$(BUILDDIR)/,,$(subst .o,.c,$@)) 
	mkdir -p $(LIBDEST); mv $(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) $(LIBDEST)/$(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) 
	
$(AXTLS_LIB_OBJS): 
	$(CC) $(SDK_CCOPTS) $(DEFS) -c $(subst obj/$(BUILDDIR)/,,$(subst .o,.c,$@)) 
	mkdir -p $(LIBDEST)/axtls; mv $(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) $(LIBDEST)/axtls/$(notdir $(subst obj/$(BUILDDIR)/,,$(subst .c,.o, $@))) 	

app: $(LIBNAME)
	$(CC) $(SDK_CCOPTS) -I $(TW_APP_INC_DIR) $(DEFS) -c $(patsubst %,$(TW_APP_SRC_DIR)/%,$(TW_APP_SRC)) 
	mv *.o  $(LIBDEST)
	$(LINKER) $(LIBOPTS) -L$(LIBDIR) -L$(LIBDEST) $(patsubst %.c,obj/$(BUILDDIR)/%.o,$(notdir $(TW_APP_SRC)))  $(STATIC_LIBS) -l$(TW_SDK_LIB_NAME) $(LIB_DEPS) 
	mkdir -p $(APPDEST) ; mv a.out $(APPDEST)/$(TW_APP_NAME); rm -f $(subst .c,.o,$(TW_APP_SRC))
