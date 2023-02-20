#CC = clang++
#
#TACO_ROOT  := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
#INCDIRS    := $(TACO_ROOT)/include $(TACO_ROOT)/external/basis/include
#CPPFLAGS   := -std=c++20
#DEFINES    :=
#LINKFLAGS  := 
#
#CPPSOURCES    := $(foreach srcdir, $(TACO_ROOT)/src, $(wildcard $(srcdir)/*.cpp))
#
#DEFINES := _DEBUG
#
#CPPSOURCES += src/posix/fiber_impl.cpp
#CPPSOURCES += external/basis/src/string.cpp
#
#DEFINES    += _XOPEN_SOURCE
#PLATFORM   := posix
#
#windows: CPPSOURCES += 
#windows: DEFINES    += BASIS_PLATFORM_WINDOWS
#windows: PLATFORM   := windows
#
#CONFIG := debug
#BUILDDIR := intermediate/$(CONFIG)/$(PLATFORM)
#LIB = $(BUILDDIR)/tacod.a
#
#OBJECTS = $(addprefix $(BUILDDIR)/, $(CPPSOURCES:.cpp=.o))
#
#posix: $(LIB) makedirs
#
#DEPENDENCIES = $(OBJECTS:.o=.d)
#-include $(DEPENDENCIES)
#
#$(LIB): $(OBJECTS)
#	ar rcs $(LIB) $^
#
#$(BUILDDIR)/%.o : %.cpp
#	$(CXX) $(CPPFLAGS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%) -c -o $@ $<
#
#$(BUILDDIR)/%.d : %.cpp
#	@mkdir -p $(dir $@)
#	@$(CXX) $(CPPFLAGS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%) -MM $< > $@
#	@cp -f $@ $@.tmp
#	@sed -e 's|.*:|$(@:.d=.o):|' < $@.tmp > $@
#	@sed -e 's/.*://' -e 's/\\$$//' < $@.tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@
#	@rm -f $@.tmp
#
#
#DIRS = $(BUILDDIR) $(OUTDIR)/$(PLATFORM)
#makedirs: $(DIRS)
#$(DIRS):
#	@mkdir -p $@
#
#.PHONY: makedirs $(DIRS)
#
#TESTS_BLOCKING = blocking
#
#$(TESTS_BLOCKING): tests/blocking.cpp
#	$(CC) tests/blocking.cpp $(CPPFLAGS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%) $(LIB) -o $@ 
#
#scheduler: tests/scheduler.cpp $(LIB)
#	$(CC) tests/scheduler.cpp $(CPPFLAGS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%) $(LIB) -o $@  
#
#work_queue: tests/work_queue.cpp $(LIB)
#	$(CC) tests/work_queue.cpp $(CPPFLAGS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%) $(LIB) -o $@  

CONFIG ?= debug
PLATFORM ?= posix

SOURCES :=
DEFINES :=
INCLUDE_DIRS :=
CPPFLAGS := -std=c++20 -Wall

ifeq ($(CONFIG),release)
	DEFINES += BUILD_CONFIG_RELEASE NDEBUG
	CPPFLAGS += -Os
else ifeq ($(CONFIG),develop)
	DEFINES += BUILD_CONFIG_DEVELOP NDEBUG
	CPPFLAGS += -Os -gfull
else ifeq ($(CONFIG),debug)
	DEFINES += BUILD_CONFIG_DEBUG _DEBUG
	CPPFLAGS += -O0 -gfull
else
	$(error Unrecognized CONFIG "$(CONFIG)" - must be release | develop | debug)
endif

INTERMEDIATE_ROOT := intermediate
OUTPUT_ROOT := bin
INTERMEDIATE_DIR := $(INTERMEDIATE_ROOT)/$(CONFIG)
OUTPUT_DIR := $(OUTPUT_ROOT)/$(CONFIG)

OBJECTS =

-include ../taco.mak

PROGRAMS := scheduler blocking future generator work_queue

scheduler: 		SOURCES += tests/scheduler.cpp
blocking: 		SOURCES += tests/blocking.cpp
future: 		SOURCES += tests/future.cpp
generator: 		SOURCES += tests/generator.cpp
work_queue: 	SOURCES += tests/work_queue.cpp

OBJECTS += $(SOURCES:%.cpp=$(INTERMEDIATE_DIR)/%.o)

-include $(OBJECTS:%.o=%.d)
-include $(INTERMEDIATE_DIR)/tests/scheduler.d
-include $(INTERMEDIATE_DIR)/tests/shared_mutex.d
-include $(INTERMEDIATE_DIR)/tests/signal.d

.SECONDEXPANSION:

$(INTERMEDIATE_DIR)/tests/%.o: %.cpp
	@echo "tests:" $<
	@mkdir -p $(dir $@)
	@$(CXX) $< $(CPPFLAGS) $(INCLUDE_DIRS:%=-I%) $(DEFINES:%=-D%) -MMD -c -o $@

$(PROGRAMS): $$(OBJECTS)
	@echo "Linking:" $@
	@mkdir -p $(OUTPUT_DIR)
	@$(CXX) $^ -o $(OUTPUT_DIR)/$@

.PHONY: clean all

clean:
	@echo "Removing intermediate files..."
	@rm -r $(INTERMEDIATE_DIR)

all: $(PROGRAMS)
