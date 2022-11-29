
INCFLAGS =

WFLAGS = -Wunused -Wunused-macros -Wunused-local-typedefs
#EXTRAFLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections

CC = clang -Wall -Wextra $(EXTRAFLAGS) $(WFLAGS)
# ricing intensifies
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
CFLAGS = $(INCFLAGS) -O0 -g3 -ggdb3
LDFLAGS = -flto -ldl -lm  -lreadline -lpthread
target = run

srcfiles_all = $(wildcard *.c)
objfiles_all = $(srcfiles_all:.c=.o)
depfiles_all = $(objfiles_all:.o=.d)
protofile = prot.inc

all: $(protofile) $(target)

$(protofile): $(srcfiles_all)
	echo > $(protofile)
	cproto $(srcfiles_all) | perl -pe 's/\b_Bool\b/bool/g' > $(protofile)_tmp
	mv $(protofile)_tmp $(protofile)

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

