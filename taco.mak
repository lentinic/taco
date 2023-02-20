TACO_ROOT      		:= $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
TACO_ROOT			:= $(TACO_ROOT:%/=%)

 -include $(TACO_ROOT)/external/basis/basis.mak

TACO_SOURCES   		:= $(foreach srcdir, $(TACO_ROOT)/src, $(wildcard $(srcdir)/*.cpp))
TACO_SOURCES		:= $(TACO_SOURCES:$(TACO_ROOT)/%=%)

TACO_INCLUDE_DIR	:= $(TACO_ROOT)/include
TACO_DEFINES		:=
TACO_CPPFLAGS		:=

ifeq ($(PLATFORM),posix)
	TACO_DEFINES += _XOPEN_SOURCE
	TACO_SOURCES += src/posix/fiber_impl.cpp
else ifeq ($(PLATFORM),windows)
	TACO_SOURCES += src/windows/fiber_impl.cpp
endif

TACO_OBJECTS       	:= $(TACO_SOURCES:%.cpp=$(INTERMEDIATE_DIR)/taco/%.o)

$(TACO_OBJECTS): INCLUDE_DIRS := $(TACO_INCLUDE_DIR)
$(TACO_OBJECTS): INCLUDE_DIRS += $(BASIS_INCLUDE_DIR)
$(TACO_OBJECTS): DEFINES += $(TACO_DEFINES)
$(TACO_OBJECTS): CPPFLAGS += $(TACO_CPPFLAGS)

$(INTERMEDIATE_DIR)/taco/%.o : $(TACO_ROOT)/%.cpp
	@echo "taco:" $(<:$(TACO_ROOT)/%=%)
	@mkdir -p $(dir $@)
	@$(CXX) $< $(CPPFLAGS) $(INCLUDE_DIRS:%=-I%) $(DEFINES:%=-D%) -MMD -c -o $@

OBJECTS += $(TACO_OBJECTS)
INCLUDE_DIRS += $(TACO_INCLUDE_DIR)
