.RECIPEPREFIX = >
CC      = $(DEVKITXENON)/bin/xenon-gcc
MACH    = -m32 -mno-altivec -fno-pic -mpowerpc64 -DXENON
CFLAGS  = -O2 -Wall $(MACH) -I$(DEVKITXENON)/usr/include
LDS     = $(wildcard $(DEVKITXENON)/app.lds)
LWIP    = $(if $(wildcard $(DEVKITXENON)/usr/lib/liblwip.a),-llwip)
LDFLAGS = $(MACH) -n $(if $(LDS),-T $(LDS)) -L$(DEVKITXENON)/usr/lib -lxenon $(LWIP) -lm
SRC     = src/main.c src/ws.c src/platform_xenon.c
$(info script de ligacao: $(LDS))
all: nimbo.elf
nimbo.elf: $(SRC)
> $(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
