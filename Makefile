SRCS = \
	server.c \
	picohttpparser.c \
	neco.c

OBJS = $(subst .c,.o,$(SRCS))

CFLAGS = -I. -O2
LIBS = 
TARGET = neco-http-server
ifeq ($(OS),Windows_NT)
TARGET := $(TARGET).exe
endif

.SUFFIXES: .c .o

all : $(TARGET)

$(TARGET) : $(OBJS)
	gcc -o $@ $(OBJS) $(LIBS)

.c.o :
	gcc -c $(CFLAGS) -I. $< -o $@

clean :
	rm -f *.o $(TARGET)
