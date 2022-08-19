GITHUB_DEPS += simplerobot/build-scripts
GITHUB_DEPS += simplerobot/logger
GITHUB_DEPS += simplerobot/test
GITHUB_DEPS += simplerobot/rlm3-base
GITHUB_DEPS += simplerobot/rlm3-driver-base-sim
GITHUB_DEPS += simplerobot/rlm3-driver-flash-sim
GITHUB_DEPS += simplerobot/rlm3-firmware-base
include ../build-scripts/build/release/include.make

CPU_CC = g++
CPU_CFLAGS = -Wall -Werror -pthread -DTEST -fsanitize=address -static-libasan -g -Og

SOURCE_DIR = source
MAIN_SOURCE_DIR = $(SOURCE_DIR)/main
CPU_TEST_SOURCE_DIR = $(SOURCE_DIR)/test-cpu

BUILD_DIR = build
LIBRARY_BUILD_DIR = $(BUILD_DIR)/library
CPU_TEST_BUILD_DIR = $(BUILD_DIR)/test-cpu
RELEASE_DIR = $(BUILD_DIR)/release

LIBRARY_FILES = $(notdir $(wildcard $(MAIN_SOURCE_DIR)/*))

CPU_TEST_SOURCE_DIRS = $(MAIN_SOURCE_DIR) $(CPU_TEST_SOURCE_DIR) $(PKG_LOGGER_DIR) $(PKG_TEST_DIR) $(PKG_RLM3_BASE_DIR) $(PKG_RLM3_DRIVER_BASE_SIM_DIR) $(PKG_RLM3_DRIVER_FLASH_SIM_DIR) $(PKG_RLM3_FIRMWARE_BASE_DIR)
CPU_TEST_SOURCE_FILES = $(notdir $(wildcard $(CPU_TEST_SOURCE_DIRS:%=%/*.c) $(CPU_TEST_SOURCE_DIRS:%=%/*.cpp)))
CPU_TEST_O_FILES = $(addsuffix .o,$(basename $(CPU_TEST_SOURCE_FILES)))
CPU_INCLUDES = $(CPU_TEST_SOURCE_DIRS:%=-I%)

VPATH = $(MCU_TEST_SOURCE_DIRS) $(CPU_TEST_SOURCE_DIRS)

.PHONY: default all library test-cpu release clean

default : all

all : release

library : $(LIBRARY_FILES:%=$(LIBRARY_BUILD_DIR)/%)

$(LIBRARY_BUILD_DIR)/% : $(MAIN_SOURCE_DIR)/% | $(LIBRARY_BUILD_DIR)
	cp $< $@

$(LIBRARY_BUILD_DIR) :
	mkdir -p $@

test-cpu : library $(CPU_TEST_BUILD_DIR)/a.out
	$(CPU_TEST_BUILD_DIR)/a.out

$(CPU_TEST_BUILD_DIR)/a.out : $(CPU_TEST_O_FILES:%=$(CPU_TEST_BUILD_DIR)/%)
	$(CPU_CC) $(CPU_CFLAGS) $^ -o $@

$(CPU_TEST_BUILD_DIR)/%.o : %.cpp Makefile | $(CPU_TEST_BUILD_DIR)
	$(CPU_CC) -c $(CPU_CFLAGS) $(CPU_INCLUDES) -MMD $< -o $@
	
$(CPU_TEST_BUILD_DIR)/%.o : %.c Makefile | $(CPU_TEST_BUILD_DIR)
	$(CPU_CC) -c $(CPU_CFLAGS) $(CPU_INCLUDES) -MMD $< -o $@

$(CPU_TEST_BUILD_DIR) :
	mkdir -p $@

release : test-cpu $(LIBRARY_FILES:%=$(RELEASE_DIR)/%)

$(RELEASE_DIR)/% : $(LIBRARY_BUILD_DIR)/% | $(RELEASE_DIR)
	cp $< $@
	
$(RELEASE_DIR) :
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(CPU_TEST_BUILD_DIR)/*.d)


