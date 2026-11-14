# Makefile del launcher: el mismo talle del ejemplo (launcher.c + las librerias
# de gestos y de banda de estado + start.S, enlazados con 'ld -r').  Ver
# apps/ejemplo/Makefile para el por que de cada bandera del ABI.

TOPDIR = ../..

CC = riscv32-unknown-elf-gcc
LD = riscv32-unknown-elf-ld

CFLAGS = -Os -fno-builtin -fno-pic -ffunction-sections -std=gnu11 \
         -march=rv32imaf_zicsr_zifencei -mabi=ilp32f -mno-relax \
         -msmall-data-limit=0 -Wall \
         -I$(TOPDIR) -I$(TOPDIR)/include -I$(TOPDIR)/lvgl -I$(TOPDIR)/fatfs \
         -I$(TOPDIR)/system/platforms/esp32p4 -I../lib-fixed

APP  = launcher
OBJS = launcher.o ../lib-fixed/gestos.o ../lib-fixed/estado.o ../lib-fixed/start.o

all: $(APP).elf

$(APP).elf: $(OBJS)
	$(LD) -r -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

../lib-fixed/gestos.o: ../lib-fixed/gestos.c ../lib-fixed/gestos.h
	$(CC) $(CFLAGS) -c -o $@ $<

../lib-fixed/estado.o: ../lib-fixed/estado.c ../lib-fixed/estado.h
	$(CC) $(CFLAGS) -c -o $@ $<

../lib-fixed/start.o: ../lib-fixed/start.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f launcher.o ../lib-fixed/gestos.o ../lib-fixed/estado.o ../lib-fixed/start.o launcher.elf

.PHONY: all clean
