
INCFLAGS =

WFLAGS = -Wunused -Wunused-macros -Wunused-local-typedefs

## used to find dead code. use sparingly.
#EXTRAFLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections
#EXTRAFLAGS = -fsanitize=address

## don't ever remove '-Wall -Wextra' - it's the bare minimum!
CC = gcc  -Wall -Wextra $(EXTRAFLAGS) $(WFLAGS)

## only ever increase -O when in release. -O0 disables inlining entirely, which is needed for debugging.
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
CFLAGS = $(INCFLAGS) -O0 -g3 -ggdb3
LDFLAGS = -flto -ldl -lm  -lreadline -lpthread
target = run

srcfiles_all = $(wildcard *.c)
objfiles_all = $(srcfiles_all:.c=.o)
depfiles_all = $(objfiles_all:.o=.d)
protofile = prot.inc

# janky mess
havecproto = 1
ifeq (, $(shell which cprotod))
havecproto = 0
endif

all: $(protofile) $(target)

## dear god what a kludge
ifeq (1, $(havecproto))
$(protofile): $(srcfiles_all)
	echo > $(protofile)
	cproto $(srcfiles_all) | perl -pe 's/\b_Bool\b/bool/g' > $(protofile)_tmp
	mv $(protofile)_tmp $(protofile)
else
$(protofile): $(srcfiles_all)
endif

$(target): $(protofile) $(objfiles_all)
	$(CC) -o $@ $(objfiles_all) $(LDFLAGS)

-include $(depfiles_all)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.o: %.c
	$(CC) $(CFLAGS) -c $(DBGFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(objfiles_all) $(target)

.PHONY: cleandep
cleandep:
	rm -f $(depfiles_all)

.PHONY: rebuild
rebuild: clean cleandep $(target)

