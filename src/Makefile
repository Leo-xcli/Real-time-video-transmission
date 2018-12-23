# ----------------------------------------------------------------------------
# Makefile for building tapp
#
# Copyright 2010 FriendlyARM (http://www.arm9.net/)
#

CFLAGS				= -Wall -O2
CC					= arm-linux-g++
C					= arm-linux-gcc
TARGET				= mfc_enc


all: $(TARGET)

mfc_enc: main.o mfc.o SsbSipMfcEncAPI.o camera.o rtpSend.o
	$(CC) $(CFLAGS) $^ -o $@

main.o:
	$(CC) $(CFLAGS) -c main.cpp

mfc.o:
	$(CC) $(CFLAGS) -c mfc.cpp
	
SsbSipMfcEncAPI.o:
	$(C) $(CFLAGS) -c SsbSipMfcEncAPI.c
	
camera.o:
	$(CC) $(CFLAGS) -c camera.cpp

rtpSend.o:
	$(CC) $(CFLAGS) -c rtpSend.cpp

clean distclean:
	rm -rf *.o $(TARGET)


# ----------------------------------------------------------------------------
# End of file
# vim: syntax=make

