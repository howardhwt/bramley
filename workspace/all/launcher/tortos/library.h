/* Shim for TortOS's library.h. The vendored muselib.h only needs LIB_PATH from it, the size of a
 * path buffer, so this supplies the same value instead of bringing in TortOS's ROM library. */
#ifndef TORTOS_LIBRARY_H
#define TORTOS_LIBRARY_H
#define LIB_PATH 544
#endif
