/* jconfig.h - libjpeg configuration for Windows/MSVC */
/* Based on jconfig.vc from libjpeg 9f (IJG) */

#ifndef JCONFIG_H
#define JCONFIG_H

/* Use Windows-compatible settings */
#define HAVE_PROTOTYPES 1
#define HAVE_UNSIGNED_CHAR 1
#define HAVE_UNSIGNED_SHORT 1
/* #define void char */
/* #define const */

/* Define this if you want to allow an alternate max memory config file */
/* #define JPEG_INTERNALS */

/* Define this if your system has an ANSI-conforming <stddef.h> file */
#define HAVE_STDDEF_H 1

/* Define this if your system has an ANSI-conforming <stdlib.h> file */
#define HAVE_STDLIB_H 1

/* Define this if your system does not have an ANSI/SysV <string.h>,
 * but does have a BSD-style <strings.h>.
 */
/* #define NEED_BSD_STRINGS */

/* Define this if your system does not provide typedef size_t in any of the
 * ANSI-standard places (stddef.h, stdlib.h, or stdio.h), but places it in
 * <sys/types.h> instead.
 */
/* #define NEED_SYS_TYPES_H */

/* For 80x86 machines, you need to define NEED_FAR_POINTERS,
 * unless you are using a large-data memory model or 80386 flat-memory mode.
 * On less brain-damaged CPUs this symbol must not be defined.
 * (Defining this symbol causes large data structures to be referenced through
 * "far" pointers and to be allocated with a special version of malloc.)
 */
/* #define NEED_FAR_POINTERS */

/* Define this if your linker needs global names to be unique in less
 * than the first 15 characters.
 */
/* #define NEED_SHORT_EXTERNAL_NAMES */

/* Although a real pointless one, this is necessary for correct operation */
/* #define INCOMPLETE_TYPES_BROKEN */

/* Define "boolean" as unsigned char, not int, on Windows systems */
#ifdef _WIN32
#ifndef __RPCNDR_H__     /* don't conflict if rpcndr.h already read */
typedef unsigned char boolean;
#endif
#ifndef FALSE            /* in case these macros already exist */
#define FALSE   0        /* values of boolean */
#endif
#ifndef TRUE
#define TRUE    1
#endif
#define HAVE_BOOLEAN     /* prevent jmorecfg.h from redefining it */
#endif

/* Define this if you get warnings about undefined structures */
#define JPEG_INTERNALS

/* Define this to support 12-bit sample values */
/* #define BITS_IN_JSAMPLE 12 */

/* Define this for the Microsoft Visual C++ compiler */
#ifdef _MSC_VER
#define INLINE __inline
#else
#define INLINE inline
#endif

#endif /* JCONFIG_H */

