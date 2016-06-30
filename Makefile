# Makefile for 7cc

# version
MAJOR = 0
MINOR = 3
FIXES = 0
EXTRAVERSION =

CFLAGS = -Wall -std=c99
LDFLAGS =
7CC = 7cc
CC1 = cc1
utils_dir = utils/
sys_dir = sys/
UTILS_OBJ =
UTILS_INC =
LIBCPP_OBJ =
LIBCPP_INC =
CC1_OBJ =
CC1_INC =
SYS_OBJ =
SYS_INC =
CONFIG_FLAGS =
KERNEL := $(shell uname)
RM = @rm -f
CONFIG_H = config.h
BUILD_DIR = "$(shell pwd)"

UTILS_OBJ += $(utils_dir)wrapper.o
UTILS_OBJ += $(utils_dir)strbuf.o
UTILS_OBJ += $(utils_dir)vector.o
UTILS_OBJ += $(utils_dir)map.o
UTILS_OBJ += $(utils_dir)string.o
UTILS_OBJ += $(utils_dir)set.o

UTILS_INC += $(utils_dir)strbuf.h
UTILS_INC += $(utils_dir)vector.h
UTILS_INC += $(utils_dir)map.h
UTILS_INC += $(utils_dir)set.h
UTILS_INC += $(utils_dir)utils.h

LIBCPP_OBJ += cpp.o
LIBCPP_OBJ += hideset.o
LIBCPP_OBJ += imap.o
LIBCPP_OBJ += input.o
LIBCPP_OBJ += lex.o

LIBCPP_INC += lex.h
LIBCPP_INC += token.def
LIBCPP_INC += cpp.h

CC1_OBJ += alloc.o
CC1_OBJ += error.o
CC1_OBJ += ast.o
CC1_OBJ += cc.o
CC1_OBJ += print.o
CC1_OBJ += decl.o
CC1_OBJ += eval.o
CC1_OBJ += expr.o
CC1_OBJ += gen.o
CC1_OBJ += stmt.o
CC1_OBJ += sym.o
CC1_OBJ += type.o
CC1_OBJ += ir.o
CC1_OBJ += block.o

CC1_INC += error.h
CC1_INC += cc.h
CC1_INC += ast.h
CC1_INC += color.h
CC1_INC += gen.h
CC1_INC += node.def
CC1_INC += rop.def

SYS_INC += $(sys_dir)sys.h

7CC_OBJ = 7cc.o

ifneq (, ${STAGE})

CONFIG_FLAGS += -DSTAGE=${STAGE}

else

CFLAGS += -g
# CFLAGS += -pg
# LDFLAGS += -pg

endif

ifeq (Linux, $(KERNEL))

SYS_OBJ += $(sys_dir)unix.o
SYS_OBJ += $(sys_dir)linux.o

cpu_has = $(shell cat /proc/cpuinfo|grep ^flags|awk '{if (match($$0, $(1))) {print 1} else {print 0}}')
HAVE_MMX = $(call cpu_has, /mmx/)
HAVE_SSE = $(call cpu_has, /sse/)
HAVE_SSE2 = $(call cpu_has, /sse2/)
HAVE_SSE4_2 = $(call cpu_has, /sse4_2/)
HAVE_AVX = $(call cpu_has, /avx/)

ifeq ($(HAVE_SSE4_2), 1)

CFLAGS += -msse4.2

endif

else ifeq (Darwin, $(KERNEL))

SYS_OBJ += $(sys_dir)unix.o
SYS_OBJ += $(sys_dir)darwin.o
XCODE_SDK_DIR := $(shell xcrun --show-sdk-path)
OSX_SDK_VERSION := $(shell xcrun --show-sdk-version)
cpu_has = $(shell sysctl -n machdep.cpu.features|awk '{if (match($$0, $(1))) {print 1} else {print 0}}')
HAVE_MMX = $(call cpu_has, /MMX/)
HAVE_SSE = $(call cpu_has, /SSE/)
HAVE_SSE2 = $(call cpu_has, /SSE2/)
HAVE_SSE4_2 = $(call cpu_has, /SSE4.2/)
HAVE_AVX = $(call cpu_has, /AVX1.0/)

else

$(error unsupported platform '$(KERNEL)')

endif

CFLAGS += $(CONFIG_FLAGS)

all:: $(CONFIG_H) $(7CC) $(CC1)

$(7CC): $(7CC_OBJ) $(UTILS_OBJ) $(SYS_OBJ)
	$(CC) $(7CC_OBJ) $(SYS_OBJ) $(UTILS_OBJ) $(LDFLAGS) -o $@

$(CC1): $(CC1_OBJ) $(LIBCPP_OBJ) $(UTILS_OBJ) $(SYS_OBJ)
	$(CC) $(CC1_OBJ) $(LIBCPP_OBJ) $(UTILS_OBJ) $(SYS_OBJ) $(LDFLAGS) -o $@

$(CC1_OBJ): $(CC1_INC) $(CONFIG_H)

$(SYS_OBJ): $(SYS_INC) $(CONFIG_H)

$(UTILS_OBJ): $(UTILS_INC) $(CONFIG_H)

$(LIBCPP_OBJ): $(LIBCPP_INC) $(CONFIG_H)

$(CONFIG_H):
	@echo "/* Auto-generated by makefile. */" > $@
	@echo "#ifndef _CONFIG_H" >> $@
	@echo "#define _CONFIG_H" >> $@
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
ifeq ($(HAVE_MMX), 1)
	@echo "#define HAVE_MMX 1" >> $@
endif
ifeq ($(HAVE_SSE), 1)
	@echo "#define HAVE_SSE 1" >> $@
endif
ifeq ($(HAVE_SSE2), 1)
	@echo "#define HAVE_SSE2 1" >> $@
endif
ifeq ($(HAVE_SSE4_2), 1)
	@echo "#define HAVE_SSE4_2 1" >> $@
endif
ifeq ($(HAVE_AVX), 1)
	@echo "#define HAVE_AVX 1" >> $@
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

TESTS := $(patsubst %.c, %.bin, $(wildcard test/test_*.c))

test/%.o: test/%.c
	$(CC) -Wall -std=c99 -o $@ -c $<

test/%.bin: test/%.o test/main.o $(UTILS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $< test/main.o $(UTILS_OBJ)

test:: $(TESTS)
	@for test in $(TESTS); do \
		./$$test || exit; \
	done

objclean::
	$(RM) *.o *~
	$(RM) $(sys_dir)*.o $(sys_dir)*~
	$(RM) $(utils_dir)*.o $(utils_dir)*~
	$(RM) $(TESTS) test/*.o test/*~
	$(RM) include/*~ 7cc.exe*

clean:: objclean
	$(RM) $(7CC) $(CC1)
	$(RM) stage1 stage2 stage3 cc1_stage1 cc1_stage2 cc1_stage3 $(CONFIG_H)

