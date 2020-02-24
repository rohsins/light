#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := ProjectRemoteTest
EXTRA_COMPONENT_DIRS += ../rLibrary/

# LIB_DIRS += $(dir $(LIB_DEPS))
# LIB_DEPS += $(PROJECT_PATH)/build/libalgobsec.a
# LDFLAGS += $(PROJECT_PATH)/build/libalgobsec.a -Wl,--no-whole-archive
# LIBC_PATH := $(PROJECT_PATH)/build/libalgobsec.a
# LIBM_PATH := $(PROJECT_PATH)/build/libalgobsec.a

EXCLUDE_COMPONENTS := aws_iot bt coap cxx esp_https_ota jsmn libsodium ulp

include $(IDF_PATH)/make/project.mk

