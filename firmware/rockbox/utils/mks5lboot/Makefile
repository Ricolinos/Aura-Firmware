#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
CC := gcc
CFLAGS += -Wall -Wextra

OUTPUT = mks5lboot

# inputs for lib
LIBSOURCES := dualboot.c mkdfu.c ipoddfu.c
# inputs for binary only
SOURCES := $(LIBSOURCES) main.c
# dependencies for binary
EXTRADEPS :=

CPPDEFINES := $(shell echo foo | $(CROSS)$(CC) -dM -E -)

ifeq ($(findstring __WIN32__,$(CPPDEFINES)),__WIN32__)
LDOPTS += -lsetupapi
# optional libusb support (needed for WinUSB and libusbK drivers)
ifeq ($(findstring MINGW,$(CPPDEFINES)),MINGW)
ifeq ($(USE_LIBUSBAPI),1)
CFLAGS += -DUSE_LIBUSBAPI
LDOPTS += -Wl,-Bstatic -lusb-1.0
endif
endif
else
ifeq ($(findstring APPLE,$(CPPDEFINES)),APPLE)
LDOPTS += -L/usr/local/lib -framework IOKit -framework CoreFoundation
# Modificado por el proyecto Aura, 2026-08-10 (GPL v2 s2a; ver
# DECISIONS.md D-050 y MODIFICATIONS.md). El resto de este archivo es
# el Makefile original de Rockbox sin cambios.
#
# backend libusb opcional ademas de IOKit (no lo reemplaza): el
# backend IOKit clasico (IOUSBDeviceInterface) puede sufrir timeouts
# de transaccion bajo el stack USB moderno de macOS con dispositivos
# en modo DFU/WTF; libusb usa una ruta distinta que a veces evita ese
# problema. api_list[] prueba ambos backends en orden, asi que agregar
# este no rompe nada si IOKit ya funciona.
ifeq ($(USE_LIBUSBAPI),1)
CFLAGS += -DUSE_LIBUSBAPI -I$(shell brew --prefix libusb 2>/dev/null || echo /opt/homebrew)/include
LDOPTS += -L$(shell brew --prefix libusb 2>/dev/null || echo /opt/homebrew)/lib -lusb-1.0
endif
else # Linux
CFLAGS += -DUSE_LIBUSBAPI
LDOPTS += -lusb-1.0
endif
endif

include ../libtools.make

# explicit dependencies on dualboot.{c,h} and mks5lboot.h
$(OBJDIR)mks5lboot.o: dualboot.h dualboot.c mks5lboot.h
$(OBJDIR)main.o: dualboot.h dualboot.c main.c mks5lboot.h
