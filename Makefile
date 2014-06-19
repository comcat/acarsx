# Copyright (c) 2014 Jack Tan(BH1RBH) at Jack's Lab

CFLAGS= -Ofast -ftree-vectorize -funroll-loops -pthread
LDLIBS= -lm -pthread  -lrtlsdr

acarsx:	main.o acars.o msk.o rtl.o output.o
	$(CC) main.o acars.o msk.o rtl.o output.o -o $@ $(LDLIBS)

main.o:	main.c acarsx.h
acars.o:	acars.c acarsx.h syndrom.h
msk.o:	msk.c acarsx.h
output.o:	output.c acarsx.h
rtl.o:	rtl.c acarsx.h

clean:
	@rm *.o acarsx
