CC=g++
CXXFLAGS= -Wall -Wvla -Wextra -Werror -g -std=c++17
LDFLAGS= -lm
SRC1= container.cpp
SRC2= sockets.cpp


OBJS1=$(SRC1:.cpp=.o)
OBJS2=$(SRC2:.cpp=.o)

CONT_EXE = container
SOC_EXE = sockets
TARGETS1 = $(CONT_EXE)
TARGETS2 = $(SOC_EXE)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS=$(SRC1) $(SRC2) Makefile README


all: $(OBJS1) $(OBJS2)
	$(CC) $(OBJS1) -o $(TARGETS1)
	$(CC) $(OBJS2) -o $(TARGETS2)

$(OBJS1) : $(SRC1)
	$(CC) -o $@ -c $^

$(OBJS2) : $(SRC2)
	$(CC) -o $@ -c $^

clean:
	$(RM) $(TARGETS1)  $(OBJS1) $(TARGETS2)  $(OBJS2)  *~ *core


tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)


