.RECIPEPREFIX = >
CC      = $(DEVKITXENON)/bin/xenon-gcc
MACH    = -m32 -mno-altivec -fno-pic -mpowerpc64 -mhard-float -DXENON
CFLAGS  = -O2 -Wall $(MACH) -I$(DEVKITXENON)/usr/include
LDS     = $(wildcard $(DEVKITXENON)/app.lds)
LDFLAGS = $(MACH) -n $(if $(LDS),-T $(LDS)) -u read -u _start -u exc_base -L$(DEVKITXENON)/xenon/lib/32 -L$(DEVKITXENON)/usr/lib -lxenon -lm
SRC     = src/main.c src/ws.c src/platform_xenon.c
$(info MAKEFILE NOVO - script de ligacao: $(LDS))
all: nimbo.elf
nimbo.elf: $(SRC)
> $(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
