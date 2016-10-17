# Makefile for 7cc

# version
MAJOR = 0
MINOR = 4
FIXES = 0
EXTRAVERSION = -dev

CFLAGS = -Wall -std=c99 -I.
LDFLAGS =
7CC = 7cc
CC1 = cc1
libutils_dir = libutils/
libcpp_dir = libcpp/
LIBCPP = libcpp.a
LIBUTILS = libutils.a
LIBUTILS_OBJ =
LIBUTILS_INC =
LIBCPP_OBJ =
LIBCPP_INC =
CC1_OBJ =
CC1_INC =
CONFIG_FLAGS =
KERNEL := $(shell uname)
RM = @rm -f
AR = ar
ARFLAGS = cru
CONFIG_H = config.h
BUILD_DIR = "$(shell pwd)"

LIBUTILS_OBJ += $(libutils_dir)alloc.o
LIBUTILS_OBJ += $(libutils_dir)wrapper.o
LIBUTILS_OBJ += $(libutils_dir)strbuf.o
LIBUTILS_OBJ += $(libutils_dir)vector.o
LIBUTILS_OBJ += $(libutils_dir)map.o
LIBUTILS_OBJ += $(libutils_dir)string.o
LIBUTILS_OBJ += $(libutils_dir)set.o
LIBUTILS_OBJ += $(libutils_dir)list.o
LIBUTILS_OBJ += $(libutils_dir)sys.o

LIBUTILS_INC += $(libutils_dir)utils.h
LIBUTILS_INC += $(libutils_dir)strbuf.h
LIBUTILS_INC += $(libutils_dir)vector.h
LIBUTILS_INC += $(libutils_dir)map.h
LIBUTILS_INC += $(libutils_dir)set.h
LIBUTILS_INC += $(libutils_dir)list.h
LIBUTILS_INC += $(libutils_dir)sys.h
LIBUTILS_INC += $(libutils_dir)color.h

LIBCPP_OBJ += $(libcpp_dir)lex.o
LIBCPP_OBJ += $(libcpp_dir)cpp.o
LIBCPP_OBJ += $(libcpp_dir)hideset.o
LIBCPP_OBJ += $(libcpp_dir)imap.o
LIBCPP_OBJ += $(libcpp_dir)input.o
LIBCPP_OBJ += $(libcpp_dir)error.o
LIBCPP_OBJ += $(libcpp_dir)expr.o

LIBCPP_INC += $(libcpp_dir)lex.h
LIBCPP_INC += $(libcpp_dir)token.def
LIBCPP_INC += $(libcpp_dir)internal.h

CC1_OBJ += error.o
CC1_OBJ += ast.o
CC1_OBJ += cc.o
CC1_OBJ += symtab.o
CC1_OBJ += type.o
CC1_OBJ += decl.o
CC1_OBJ += expr.o
CC1_OBJ += stmt.o
CC1_OBJ += sema.o
CC1_OBJ += init.o
CC1_OBJ += eval.o
CC1_OBJ += print.o
CC1_OBJ += x86_64-linux.o

CC1_INC += cc.h
CC1_INC += node.def

7CC_OBJ = 7cc.o

ifneq (, ${STAGE})
CONFIG_FLAGS += -DSTAGE=${STAGE}
else
CFLAGS += -g
# CFLAGS += -pg
# LDFLAGS += -pg
endif

ifeq (Linux, $(KERNEL))

else ifeq (Darwin, $(KERNEL))
XCODE_SDK_DIR := $(shell xcrun --show-sdk-path)
OSX_SDK_VERSION := $(shell xcrun --show-sdk-version)
else
$(error unsupported platform '$(KERNEL)')
endif

CFLAGS += $(CONFIG_FLAGS)

all:: $(CONFIG_H) $(7CC) $(CC1)

$(7CC): $(LIBUTILS) $(7CC_OBJ)
	$(CC) $(7CC_OBJ) $(LIBUTILS) $(LDFLAGS) -o $@

$(CC1): $(LIBUTILS) $(LIBCPP) $(CC1_OBJ)
	$(CC) $(CC1_OBJ) $(LIBCPP) $(LIBUTILS) $(LDFLAGS) -o $@

$(CC1_OBJ): $(CC1_INC) $(CONFIG_H)

$(LIBUTILS): $(LIBUTILS_INC) $(CONFIG_H) $(LIBUTILS_OBJ)
	$(AR) $(ARFLAGS) $@ $(LIBUTILS_OBJ)

$(LIBCPP): $(LIBCPP_INC) $(CONFIG_H) $(LIBCPP_OBJ)
	$(AR) $(ARFLAGS) $@ $(LIBCPP_OBJ)

$(CONFIG_H):
	@echo "/* Auto-generated by makefile. */" > $@
	@echo "#ifndef CONFIG_H" >> $@
	@echo "#define CONFIG_H" >> $@
	@echo >> $@
	@echo "#define VERSION \"$(MAJOR).$(MINOR).$(FIXES)$(EXTRAVERSION)\"" >> $@
	@echo "#define BUILD_DIR \"$(BUILD_DIR)\"" >> $@
ifeq (Linux, $(KERNEL))
	@echo "#define CONFIG_LINUX" >> $@
	@echo "#define CONFIG_COLOR_TERM" >> $@
else ifeq (Darwin, $(KERNEL))
	@echo "#define CONFIG_DARWIN" >> $@
	@echo "#define CONFIG_COLOR_TERM" >> $@
	@echo "#define XCODE_DIR \"$(XCODE_SDK_DIR)\"" >> $@
	@echo "#define OSX_SDK_VERSION \"$(OSX_SDK_VERSION)\"" >> $@
endif
	@echo >> $@
	@echo "#endif" >> $@

#
# Bootstrap
#
stage1:
	$(MAKE) objclean
	$(MAKE) CC=cc STAGE=1
	mv 7cc stage1
	mv cc1 cc1_stage1
	ln -s cc1_stage1 cc1

stage2: stage1
	$(MAKE) objclean
	$(MAKE) CC=./stage1 STAGE=2
	mv 7cc stage2
	mv cc1 cc1_stage2
	ln -s cc1_stage2 cc1

stage3: stage2
	$(MAKE) objclean
	$(MAKE) CC=./stage2 STAGE=3
	mv 7cc stage3
	mv cc1 cc1_stage3
	ln -s cc1_stage3 cc1

bootstrap: stage3
	cmp stage2 stage3
	cmp cc1_stage2 cc1_stage3

objclean::
	$(RM) *.o *.a *~
	$(RM) $(libutils_dir)*.o $(libutils_dir)*~
	$(RM) $(libcpp_dir)*.o $(libcpp_dir)*~
	$(RM) include/*~

clean:: objclean
	$(RM) $(7CC) $(CC1)
	$(RM) stage1 stage2 stage3 cc1_stage1 cc1_stage2 cc1_stage3 $(CONFIG_H)

