.RECIPEPREFIX = >
CC      = $(DEVKITXENON)/bin/xenon-gcc
CFLAGS  = -O2 -Wall -I$(DEVKITXENON)/usr/include
LDFLAGS = -L$(DEVKITXENON)/usr/lib -lxenon -ljpeg -lm
SRC     = src/main.c src/ws.c src/platform_xenon.c
all: nimbo.elf
nimbo.elf: $(SRC)
> $(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
