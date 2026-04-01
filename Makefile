# Project Name
TARGET = Tonecrafterpedal

# Sources
CPP_SOURCES = Tonecrafterpedal.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy/
DAISYSP_DIR = ../../DaisySP/
USE_DAISYSP_LGPL = 1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
