CONFIG_DIR    = $(HOME)/.kwm
SAMPLE_CONFIG = examples/kwmrc
DEBUG_BUILD   = -DDEBUG_BUILD -g

AXLIB_PATH    = ./lib
FRAMEWORKS    = -framework ApplicationServices -framework Carbon -framework Cocoa -L$(AXLIB_PATH) -laxlib
DEVELOPER_DIR = $(shell xcode-select -p)
SDK_ROOT      = $(DEVELOPER_DIR)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk

AXLIB_SRCS    = axlib/axlib.cpp axlib/element.cpp axlib/window.cpp axlib/application.cpp axlib/observer.cpp \
				axlib/event.cpp axlib/sharedworkspace.mm axlib/display.mm axlib/carbon.cpp
AXLIB_OBJS_TMP= $(AXLIB_SRCS:.cpp=.o)
AXLIB_OBJS    = $(AXLIB_OBJS_TMP:.mm=.o)

KWM_SRCS      = kwm/kwm.cpp kwm/container.cpp kwm/node.cpp kwm/tree.cpp kwm/window.cpp kwm/display.cpp \
				kwm/daemon.cpp kwm/interpreter.cpp kwm/keys.cpp kwm/space.cpp kwm/border.cpp kwm/cursor.cpp \
				kwm/serializer.cpp kwm/tokenizer.cpp kwm/rules.cpp kwm/scratchpad.cpp kwm/config.cpp kwm/query.cpp
KWM_OBJS_TMP  = $(KWM_SRCS:.cpp=.o)
KWM_OBJS      = $(KWM_OBJS_TMP:.mm=.o)

KWMC_SRCS     = kwmc/kwmc.cpp
KWMO_SRCS     = kwm-overlay/kwm-overlay.swift

OBJS_DIR      = ./obj
BUILD_PATH    = ./bin
BUILD_FLAGS   = -Wall
BINS          = $(BUILD_PATH)/kwm $(BUILD_PATH)/kwmc $(BUILD_PATH)/kwm-overlay $(CONFIG_DIR)/kwmrc
LIB           = $(AXLIB_PATH)/libaxlib.a

# The 'all' target builds a debug version of Kwm.
# (Re)Build AXLib if necessary, otherwise remain untouched.
all: $(BINS)

# clean all temporary build artifacts
clean: cleankwm cleanlib

# clean build artifacts related to kwm
cleankwm:
	rm -rf $(BUILD_PATH)
	rm -rf $(OBJS_DIR)/kwm

# clean build artifacts related to axlib
cleanlib:
	rm -rf $(OBJS_DIR)/axlib

# The 'install' target forces a rebuild of Kwm with the DEBUG_BUILD
# variable clear so that we don't emit debug log messages.
# (Re)Build AXLib if necessary, otherwise remain untouched.
install: BUILD_FLAGS=-O2 -Wno-deprecated
install: DEBUG_BUILD=
install: cleankwm $(BINS)

# The 'installlib' target forces a rebuild of AXLib with the DEBUG_BUILD
# variable clear so that we don't emit debug log messages.
install-lib: BUILD_FLAGS=-O2 -Wno-deprecated
install-lib: DEBUG_BUILD=
install-lib: cleanlib $(LIB)
lib: $(LIB)

.PHONY: all clean cleankwm cleanlib install lib install-lib

# This is an order-only dependency so that we create the directory if it
# doesn't exist, but don't try to rebuild the binaries if they happen to
# be older than the directory's timestamp.
$(BINS): | $(BUILD_PATH)

$(AXLIB_PATH)/libaxlib.a: $(foreach obj,$(AXLIB_OBJS),$(OBJS_DIR)/$(obj))
	@rm -rf $(AXLIB_PATH)
	@mkdir -p $(AXLIB_PATH)
	ar -cq $@ $^

$(OBJS_DIR)/axlib/%.o: axlib/%.cpp
	@mkdir -p $(@D)
	g++ -c $< $(DEBUG_BUILD) $(BUILD_FLAGS) -o $@

$(OBJS_DIR)/axlib/%.o: axlib/%.mm
	@mkdir -p $(@D)
	g++ -c $< $(DEBUG_BUILD) $(BUILD_FLAGS) -o $@

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH) && mkdir -p $(CONFIG_DIR)

$(BUILD_PATH)/kwm: $(foreach obj,$(KWM_OBJS),$(OBJS_DIR)/$(obj)) $(LIB)
	g++ $^ $(DEBUG_BUILD) $(BUILD_FLAGS) -lpthread $(FRAMEWORKS) -o $@

$(OBJS_DIR)/kwm/%.o: kwm/%.cpp
	@mkdir -p $(@D)
	g++ -c $< $(DEBUG_BUILD) $(BUILD_FLAGS) -o $@

$(OBJS_DIR)/kwm/%.o: kwm/%.mm
	@mkdir -p $(@D)
	g++ -c $< $(DEBUG_BUILD) $(BUILD_FLAGS) -o $@

$(BUILD_PATH)/kwmc: $(KWMC_SRCS)
	g++ $^ -O2 -o $@

$(BUILD_PATH)/kwm-overlay: $(KWMO_SRCS)
	swiftc $^ -static-stdlib -sdk $(SDK_ROOT) -o $@

$(CONFIG_DIR)/kwmrc: $(SAMPLE_CONFIG)
	mkdir -p $(CONFIG_DIR)
	if test ! -e $@; then cp -n $^ $@; fi
