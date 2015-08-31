EXECUTABLE    := mapred_woc

CFILES      := main.c chash.c

# Basic directory setup
ROOTDIR     ?= .
SRCDIR      ?= src
OBJDIR      ?= obj
DEPDIR      ?= dep
BINDIR      ?= .

# Compilers
CC         := gcc

# Linker
LD         := gcc

# Libraries 
LIBS       += -lm -lpthread

# Warning flags
CWARN_FLAGS :=
LDWARN_FLAGS :=

CFLAGS    :=  $(CWARN_FLAGS) $(INCLUDES)
LDFLAGS   :=  $(LDWARN_FLAGS)

# Architecture specific flags
TARGET_ARCH := -m64 -march=core2 -msse3 -mfpmath=sse 

# Compiler specific general flags
CFLAGS  		+= -std=c99

# Linker flags
LDFLAGS      += 

# Compiler specific optimization flags

# Common optimization flags
COMMONOPT_FLAGS := -O3

# Common flags for compilers and linker
COMMONFLAGS = $(TARGET_ARCH)

# Turn either debugging information or compiler optimization on
ifeq ($(debug),1)
   COMMONFLAGS += -g -D__DEBUG__
else
   COMMONFLAGS += $(COMMONOPT_FLAGS)
endif

# Turn profile information on
ifeq ($(profile),1)
   COMMONFLAGS += -pg
endif
     

TARGETDIR := $(BINDIR)
TARGET    := $(TARGETDIR)/$(EXECUTABLE)

################################################################################
# Check for input flags and set compiler flags appropriately
################################################################################

# Add common flags
CFLAGS    += $(COMMONFLAGS)
LDFLAGS   += $(COMMONFLAGS)

################################################################################
# Set up object files
################################################################################

#OBJDIR := $(ROOTOBJDIR)/$(BINSUBDIR)
OBJS :=  $(patsubst %.c,$(OBJDIR)/%.c.o,$(notdir $(CFILES)))

################################################################################
# Set up dependency files
################################################################################

DEPS :=  $(patsubst %.c,$(DEPDIR)/%.c.d,$(notdir $(CFILES)))

################################################################################
# Rules
################################################################################

.PHONY : clean

$(TARGET) : $(OBJS) Makefile
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET) 

$(OBJS) : | $(OBJDIR) $(DEPDIR)

$(OBJDIR)/%.c.o : $(SRCDIR)/%.c  Makefile
	$(CC) -MMD -MP -MF $(DEPDIR)/$(notdir $<.d) $(CFLAGS) -c $< -o $@ 

$(OBJDIR) : 
	@mkdir -p $(OBJDIR)

$(DEPDIR) :
	@mkdir -p $(DEPDIR)

clean:
	$(VERBOSE)rm -rf $(OBJDIR)
	$(VERBOSE)rm -rf $(DEPDIR)
	$(VERBOSE)rm -rf $(TARGET)

-include $(DEPS)


