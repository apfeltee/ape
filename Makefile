
EXTDIR = ./util
INCFLAGS = -I$(EXTDIR)

CC = clang
#CXX = g++ -std=c++20 -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--print-gc-sections -Wl,--demangle -Wl,-z,stack-size=0 
CXX = g++ -std=c++20
#CXX = g++ -std=c++20
# ricing intensifies
WARNFLAGS = -Wall -Wextra
#WARNFLAGS = -w
#CFLAGS = $(INCFLAGS) -Ofast -march=native -flto -ffast-math -funroll-loops
DEBUGFLAGS = -O0 -g3 -ggdb3
CFLAGS = $(INCFLAGS) $(WARNFLAGS) $(DEBUGFLAGS)
CXXFLAGS = $(CFLAGS) $(WARNFLAGS)

# disable stack entirely? it *should* match up to the ANSI C version.
EXTRALFLAGS = 

LDFLAGS = $(EXTRALFLAGS) -flto -ldl -lm  -lreadline -lpthread
#LDFLAGS =
target = run

src = \
	$(wildcard *.cpp)

obj = $(src:.cpp=.o)
dep = $(obj:.o=.d)


$(target): $(obj)
	$(CXX) -o $@ $^ $(LDFLAGS)

-include $(dep)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.cpp
	$(CXX) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) -MF $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $(DBGFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $(DBGFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(obj) $(target)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: rebuild
rebuild: clean cleandep
	$(MAKE) $(target)

.PHONY: sanity
sanity:
	./run sanity.msl