
INCFLAGS =

WFLAGS = -Wunused -Wunused-macros -Wunused-but-set-variable -Wunused-function -Wunused-label -Wunused-local-typedefs -Wunused-parameter -Wunused-value -Wunused-variable -Wunused-local-typedefs
#EXTRAFLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections

CC = gcc -Wall -Wextra  $(WFLAGS)
# ricing intensifies
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
CFLAGS = $(INCFLAGS) -O0 -g3 -ggdb3
LDFLAGS = -flto -ldl -lm  -lreadline -lpthread
target = run

src = \
	$(wildcard *.c) \

obj = $(src:.c=.o)
dep = $(obj:.o=.d)

all: prot.inc $(target)

prot.inc: builtins.c compiler.c imp.c lexer.c parser.c tostring.c
	echo > $@
	cproto $^ > $@

$(target): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(dep)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.o: %.c
	$(CC) $(CFLAGS) -c $(DBGFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(obj) $(target)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: rebuild
rebuild: clean cleandep $(target)

.PHONY: sanity
sanity:
	./run sanity.msl