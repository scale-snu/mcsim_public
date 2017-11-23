SHELL = /bin/sh
.PHONY: all depend clean
.SUFFIXES: .cc .o

default: all

ifeq ($(TAG),dbg)
OPT = -ggdb -g -O0 -fPIC
else
OPT = -O3 -g
endif

#OPT = -O3 -DNDEBUG -axS -march=core2 -mtune=core2
#OPT = -O3 -DNDEBUG -msse2 -march=pentium-m -mfpmath=sse
#CXXFLAGS = -Wall -Wno-unknown-pragmas -Winline $(OPT) 
#CXXFLAGS = -fPIC -Wno-unknown-pragmas $(OPT) 
CXXFLAGS = -Wno-unknown-pragmas $(OPT) 
CXX = g++ -DTARGET_IA32E
CC  = gcc -DTARGET_IA32E
#CXX = icpc -DTARGET_IA32E
#CC = icc -DTARGET_IA32E
PINFLAGS = 

SRCS = PTSCache.cc \
			 PTSComponent.cc \
			 PTSCore.cc \
			 PTSO3Core.cc \
			 PTSDirectory.cc \
			 PTSMemoryController.cc \
			 PTSTLB.cc \
			 PTSXbar.cc \
			 McSim.cc \
			 PTS.cc

OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(SRCS))

all: obj_$(TAG)/mcsim
	cp -f obj_$(TAG)/mcsim mcsim

obj_$(TAG)/mcsim : $(OBJS) main.cc
	$(CXX) $(CXXFLAGS) $(INCS) -o obj_$(TAG)/mcsim $(OBJS) main.cc

obj_$(TAG)/%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) $(INCS) -o $@ $<

clean:
	-rm -f *.o pin.log mcsim 

