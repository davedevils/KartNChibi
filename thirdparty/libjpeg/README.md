# libjpeg 9f (IJG - Independent JPEG Group)

This is the original libjpeg library from the Independent JPEG Group.
Version: 9f (2024-01-14)

**This software is based in part on the work of the Independent JPEG Group.**

## Source

Downloaded from: http://www.ijg.org/files/jpegsr9f.zip

## What's Included

Only the files necessary for **JPEG decoding** are included:

### Headers
- `jpeglib.h` - Main JPEG library interface
- `jconfig.h` - Platform configuration (Windows/MSVC)
- `jmorecfg.h` - Additional configuration
- `jerror.h` - Error handling
- `jinclude.h` - System includes
- `jpegint.h` - Internal definitions
- `jdct.h` - DCT definitions
- `jmemsys.h` - Memory system interface

### Source Files (Decompression)
- `jcomapi.c` - Common API routines
- `jdapimin.c` - Application interface (minimum)
- `jdapistd.c` - Application interface (standard)
- `jdatasrc.c` - Data source manager
- `jdcoefct.c` - Coefficient block controller
- `jdcolor.c` - Color conversion
- `jddctmgr.c` - DCT manager
- `jdhuff.c` - Huffman decoding
- `jdinput.c` - Input controller
- `jdmainct.c` - Main buffer controller
- `jdmarker.c` - Marker reading
- `jdmaster.c` - Master control
- `jdmerge.c` - Merged upsampling
- `jdpostct.c` - Post-processing controller
- `jdsample.c` - Upsampling
- `jerror.c` - Error handling
- `jidctflt.c` - IDCT (floating-point)
- `jidctfst.c` - IDCT (fast integer)
- `jidctint.c` - IDCT (accurate integer)
- `jmemmgr.c` - Memory manager
- `jmemnobs.c` - No backing store memory
- `jquant1.c` - 1-pass quantization
- `jquant2.c` - 2-pass quantization
- `jutils.c` - Utility routines

## Usage

### In Build System

Add all `.c` files to your build:

```cmake
file(GLOB LIBJPEG_SOURCES "${CMAKE_SOURCE_DIR}/thirdparty/libjpeg/*.c")
add_library(libjpeg STATIC ${LIBJPEG_SOURCES})
target_include_directories(libjpeg PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/libjpeg)
```

Or in a batch file:
```batch
set LIBJPEG=..\..\thirdparty\libjpeg
for %%f in (%LIBJPEG%\*.c) do cl /c /nologo %%f /Fo:obj\%%~nf.obj
```

### Enable in Engine

Define `KNC_USE_LIBJPEG_FALLBACK` when building:

```batch
set CFLAGS=/DKNC_USE_LIBJPEG_FALLBACK ...
```

## License

```
The Independent JPEG Group's JPEG software
==========================================

This distribution contains the ninth public release of the Independent JPEG
Group's free JPEG software.  You are welcome to redistribute this software and
to use it for any purpose, subject to the conditions under LEGAL ISSUES, below.

LEGAL ISSUES
============

In plain English:

1. We don't promise that this software works.
2. You can use this software for whatever you want.
3. You don't have to pay us.
4. If you claim you wrote it you'll be lying.

In legalese:

The authors make NO WARRANTY or representation, either express or implied,
with respect to this software, its quality, accuracy, merchantability, or
fitness for a particular purpose.  This software is provided "AS IS", and you,
its user, assume the entire risk as to its quality and accuracy.

This software is copyright (C) 1991-2024, Thomas G. Lane, Guido Vollbeding.
All Rights Reserved except as specified below.

Permission is hereby granted to use, copy, modify, and distribute this
software (or portions thereof) for any purpose, without fee, subject to these
conditions:
(1) If any part of the source code for this software is distributed, then this
README file must be included, with this copyright and no-warranty notice
unaltered; and any additions, deletions, or changes to the original files
must be clearly indicated in accompanying documentation.
(2) If only executable code is distributed, then the accompanying
documentation must state that "this software is based in part on the work of
the Independent JPEG Group".
(3) Permission for use of this software is granted only if the user accepts
full responsibility for any undesirable consequences; the authors accept
NO LIABILITY for damages of any kind.
```
