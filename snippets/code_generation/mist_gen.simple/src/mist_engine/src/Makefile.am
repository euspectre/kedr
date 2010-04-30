MIST_LIB_NAME = mist_engine
MIST_VERSION = $(MIST_ENGINE_VERSION_MAJOR).$(MIST_ENGINE_VERSION_MINOR).$(MIST_ENGINE_VERSION_MICRO)

VERSION_SCRIPT = $(srcdir)/versions.ldscript

###########################################################################
# MiST Engine executable
bin_PROGRAMS = mist_engine
mist_engine_SOURCES = \
	main.c		\
	grar.c		\
	mist_base.c	\
	mist_file_utils.c	\
	mist_string_utils.c	\
	mist_exec.c			\
	smap.c				\
	about.h				\
	grar.h				\
	mist_base.h			\
	mist_errors.h		\
	mist_file_utils.h	\
	mist_string_utils.h	\
	mist_exec.h	\
	smap.h	

mist_engine_CPPFLAGS = 

###########################################################################
# Platform-dependent stuff
ADD_COMPILER_FLAGS = 
ADD_LINKER_FLAGS   = 

# Additional files to be built
ADD_BUILD_FILES =

# Additional files to be removed during cleanup
ADD_CLEAN_FILES = 

# The files to be installed (in addition to the library itself)
ADD_INSTALL_FILES = 

# Only Linux and Microsoft Windows systems are currently supported
if OS_LINUX
MIST_LIB_FILE = lib$(MIST_LIB_NAME).so.$(MIST_VERSION)
MIST_LIB_SONAME = lib$(MIST_LIB_NAME).so.$(MIST_ENGINE_VERSION_MAJOR)

ADD_COMPILER_FLAGS += -fPIC
ADD_LINKER_FLAGS   += \
	-Wl,-soname,$(MIST_LIB_SONAME)	\
	-Wl,-z,defs	\
	-Wl,--version-script=$(VERSION_SCRIPT)

if GCC_HAS_FVISIBILITY
ADD_COMPILER_FLAGS += -fvisibility=hidden
endif # if GCC_HAS_FVISIBILITY
endif # if OS_LINUX

if OS_WINDOWS
MIST_LIB_FILE = $(MIST_LIB_NAME).dll
MIST_DEF_FILE = $(MIST_LIB_NAME).def

# Import library for the programs built with GCC
MIST_IMPLIB_GCC = lib$(MIST_LIB_NAME).a

# Import library for the programs built with Microsoft tools
MIST_IMPLIB_MS  = $(MIST_LIB_NAME).lib

ADD_COMPILER_FLAGS += -DMKDIR_ONE_ARG
mist_engine_CPPFLAGS += -DMKDIR_ONE_ARG

ADD_LINKER_FLAGS   += \
	-Wl,--out-implib,$(MIST_IMPLIB_GCC)	\
	-Wl,--output-def,$(MIST_DEF_FILE)

ADD_INSTALL_FILES += \
	$(MIST_DEF_FILE) \
	$(MIST_IMPLIB_GCC)

ADD_CLEAN_FILES += \
	$(MIST_DEF_FILE) \
	$(MIST_IMPLIB_GCC)

if MS_LIB_AVAILABLE
ADD_BUILD_FILES   += $(MIST_IMPLIB_MS)
ADD_INSTALL_FILES += $(MIST_IMPLIB_MS)
ADD_CLEAN_FILES   += $(MIST_IMPLIB_MS)
endif # if MS_LIB_AVAILABLE

endif # if OS_WINDOWS

if OS_LINUX
MAKE_SYMLINKS = \
	$(LN_S) "$(DESTDIR)$(libdir)/$(MIST_LIB_FILE)" "$(DESTDIR)$(libdir)/$(MIST_LIB_SONAME)" && \
	$(LN_S) "$(DESTDIR)$(libdir)/$(MIST_LIB_FILE)" "$(DESTDIR)$(libdir)/lib$(MIST_LIB_NAME).so"

RM_SYMLINKS = \
	rm -f "$(DESTDIR)$(libdir)/$(MIST_LIB_SONAME)" "$(DESTDIR)$(libdir)/lib$(MIST_LIB_NAME).so"
else
MAKE_SYMLINKS = 
RM_SYMLINKS = 
endif
###########################################################################

# A custom directory for headers
mistdir = $(includedir)/$(PACKAGE_NAME)

PRIVATE_HEADER_FILES = 	\
	grar.h				\
	mist_base.h			\
	mist_file_utils.h	\
	mist_string_utils.h	\
	smap.h				

mist_HEADERS = 	\
	mist_engine.h	\
	mist_errors.h		

# This is just for Automake to generate rules to prepare necessary
# object files (see MIST_OBJ_FILES below).
# The library itself should never be used.
noinst_LIBRARIES = libmist_engine_impl.a
libmist_engine_impl_a_SOURCES = 	\
	grar.c		\
	mist_base.c	\
	mist_file_utils.c	\
	mist_string_utils.c	\
	smap.c				\
	mist_engine.c \
	$(mist_HEADERS) \
	$(PRIVATE_HEADER_FILES)

libmist_engine_impl_a_CFLAGS = \
	-Wall -DMIST_ENGINE_DLL_EXPORTS $(ADD_COMPILER_FLAGS)

OBJPREFIX = libmist_engine_impl_a

# The list of object files needed to create the library
MIST_OBJ_FILES = \
	$(OBJPREFIX)-grar.$(OBJEXT)		\
	$(OBJPREFIX)-mist_base.$(OBJEXT)	\
	$(OBJPREFIX)-mist_file_utils.$(OBJEXT)	\
	$(OBJPREFIX)-mist_string_utils.$(OBJEXT)	\
	$(OBJPREFIX)-smap.$(OBJEXT)		\
	$(OBJPREFIX)-mist_engine.$(OBJEXT)

if OS_WINDOWS
MIST_OBJ_FILES += \
	mist_engine-win32-res.$(OBJEXT)
endif # if OS_WINDOWS

# Additional files to be distributed
ADD_EXTRA_DIST = \
	mist_engine.rc

EXTRA_DIST = \
	mist_engine.h.in 	\
	mist_engine.rc.in	\
	$(ADD_EXTRA_DIST)	\
	$(VERSION_SCRIPT)

###########################################################################
# Custom build rules
all-local: $(MIST_LIB_FILE) $(ADD_BUILD_FILES)

clean-local: 
		-rm -f $(MIST_LIB_FILE) *.exp *.$(OBJEXT) $(ADD_CLEAN_FILES)

$(MIST_LIB_FILE): $(MIST_OBJ_FILES) $(PRIVATE_HEADER_FILES) $(mist_HEADERS) $(VERSION_SCRIPT)
		$(CC_PLAIN) -shared $(ADD_LINKER_FLAGS) -o $@ $(MIST_OBJ_FILES)

if OS_WINDOWS

# Compile resources for the DLL
mist_engine-win32-res.$(OBJEXT): mist_engine.rc
		$(WINDRES) $< $@

if MS_LIB_AVAILABLE

# Generate import library
$(MIST_IMPLIB_MS): $(MIST_LIB_FILE) $(MIST_DEF_FILE)
		lib -machine:@WIN_MACHINE_ARCH@ -name:$(MIST_LIB_FILE) -def:$(MIST_DEF_FILE) -out:$@

endif # if MS_LIB_AVAILABLE
endif # if OS_WINDOWS

###########################################################################
# Custom install / uninstall rules
install-data-hook: $(MIST_LIB_FILE) $(ADD_INSTALL_FILES)
		$(mkinstalldirs) "$(DESTDIR)$(libdir)"
		$(INSTALL_DATA) $(MIST_LIB_FILE) "$(DESTDIR)$(libdir)"
		for f in $(ADD_INSTALL_FILES); do \
			if test -f "$(srcdir)/$$f"; then p="$(srcdir)/$$f"; else p="$$f"; fi; \
			$(INSTALL_DATA) "$$p" "$(DESTDIR)$(libdir)/$$f"; \
		done
		$(MAKE_SYMLINKS)

uninstall-local:
		-rm -f "$(DESTDIR)$(libdir)/$(MIST_LIB_FILE)"
		for f in $(ADD_INSTALL_FILES); do \
			rm -f "$(DESTDIR)$(libdir)/$$f"; \
		done
		$(RM_SYMLINKS)
#		-rm -r "$(DESTDIR)$(mistdir)"
