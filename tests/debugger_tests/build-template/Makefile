# Game info output to bin header.
GAME_TITLE_1 = '  UNIT   '
GAME_TITLE_2 = '  TEST   '
GAME_TITLE_3 = '         '
GAME_YEAR = $(shell date +'%Y')
GAME_MUSIC = 0xfd0d
BIN = test.bin
VEC = test.vec

CPP=/usr/local/bin/m6809-unknown-none-g++
CC1PLUS=/usr/local/libexec/gcc/m6809-unknown-none/4.3.[46]/cc1plus
# CC1=/usr/local/libexec/gcc/m6809-unknown-none/4.3.[46]/cc1

CFLAGS =

# To make a debug build: make DEBUG=1
# TODO: Output to out/Debug/ and out/Release/ so that we don't have to clean when switching config.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CFLAGS += -O0 -g
else
    CFLAGS += -DNDEBUG -O3
endif

CFLAGS += -fomit-frame-pointer
CFLAGS += -mint8
CFLAGS += -fsigned-char
#CFLAGS += -fno-inline 
CFLAGS += -fno-gcse
CFLAGS += -fno-toplevel-reorder
# Don't emit thread-safe guards around local static variables
CFLAGS += -fno-threadsafe-statics
CFLAGS += -msoft-reg-count=0
CFLAGS += -fverbose-asm -fno-time-report -fdiagnostics-show-option
CFLAGS += -W -Wall -Wextra -Wconversion -Werror -Wno-comment -Wno-unused-parameter -Wno-return-type -Wno-unused-variable

CC1FLAGS = $(CFLAGS) -quiet

AS=/usr/local/bin/as6809
AFLAGS= -l -o -s -g -p
# -l   Create list   file/outfile[.lst]
# -o   Create object file/outfile[.rel]
# -s   Create symbol file/outfile[.sym]
# -g   Undefined symbols made global
# -p   Disable automatic listing pagination

LN=/usr/local/bin/aslink
LFLAGS= -m -w -u -s -b .text=0x0
#  -m   Map output generated as (out)file[.map]
#  -w   Wide listing format for map file
#  -u   Update listing file(s) with link data as file(s)[.rst]
#  -s   Motorola S Record as (out)file[.s--]
#  -b   area base address=expression

SRCS = $(wildcard src/*.cpp)
_OBJS = $(SRCS:.cpp=.o)
OBJS = $(patsubst src/%, %, $(_OBJS))
_RSTS = $(SRCS:.cpp=.rst)
RSTS = $(patsubst src/%, %, $(_RSTS))
DEPS = $(OBJS:.o=.d)
MAP = $(BIN:.bin=.map)

.PHONY = all clean print_stats
.PRECIOUS: %.o 

# all: $(VEC) print_stats
all: test.s19

clean:
	$(RM) $(OBJS) *.o *.map *.hlr *.ram *.rom *.rst *.s *.s19 *.sym *.asm *.lst *.bin *.d *.vec

print_stats: $(MAP) crt0.asm
	@python3 ./tools/build/print_stats.py $(MAP) crt0.asm

# Rule to generate a dep file by using the C preprocessor
%.d: src/%.cpp
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

# Include generated dep files for header deps per source file
-include $(DEPS)

# Produce final .vec file from .bin
%.vec: %.bin
	# Pad to multiple of 8K
	python3 ./tools/build/pad_bin.py $< $@
	# Duplicate file to 32K
	python3 ./tools/build/dup_bin.py $@ 32

# Produce .bin file from .s19 and _ram.s19
%.bin: %.s19 %_ram.s19
	# Extract ram section into .ram
	srec_cat $*_ram.s19 -offset -0xc880 -o $*.ram -binary || echo -n
	@touch $*.ram
	# Extract rom section into .rom
	srec_cat $*.s19 -o $*.rom -binary
	# Concatenate .rom and .ram into .bin
	cat $*.rom $*.ram > $*.bin

# Link all .o files into single .s19 and _ram.s19
%.s19 %_ram.s19: $(OBJS) crt0.o
	# Link .o files to .s19, _ram.s19, and .map, and generate .rst for each .o
	$(LN) $(LFLAGS) $*.s19 crt0.o $(OBJS)
	# Demangle C/C++ names in .rst files
	$(foreach RST,$(RSTS),cat $(RST)|c++filt|sponge $(RST);)

# Produce .o from .asm
%.o: %.asm
	# Assemble .asm to .rel
	$(AS) $(AFLAGS) $<
	# Rename .rel to .o
	mv $*.rel $*.o

# Produce crt0.asm from crt0.tpl (template) by replacing placeholders with target base name
crt0.asm: make/crt0.tpl
	cat make/crt0.tpl \
		| sed -e s/GAME_TITLE_1/$(GAME_TITLE_1)/ \
		| sed -e s/GAME_TITLE_2/$(GAME_TITLE_2)/ \
		| sed -e s/GAME_TITLE_3/$(GAME_TITLE_3)/ \
		| sed -e s/GAME_YEAR/$(GAME_YEAR)/ \
		| sed -e s/GAME_MUSIC/$(GAME_MUSIC)/ > crt0.asm

%.o: src/%.cpp
	# Compile .cpp to asm file (.s)
	$(CC1PLUS) $< -dumpbase $* $(CC1FLAGS) -auxbase $* -o $*.s
	# Assemble .s to .rel, .lst, .hlr, .sym
	$(AS) $(AFLAGS) $*.s
	# Rename .rel to .o
	mv $*.rel $*.o
