include $(MK)/header.mk

SUBDIRS := iolets

TARGETS = BoundaryValues.o \
	  BoundaryComms.o 
	  
INCLUDES_$(d) := $(INCLUDES_$(parent))

include $(MK)/footer.mk
