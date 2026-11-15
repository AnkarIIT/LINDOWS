/*
 * LINDOWS bare-metal integer types.
 * Kept local so the freestanding kernel never pulls in <stdint> (no host libc).
 */
#ifndef LINDOWS_TYPES_H
#define LINDOWS_TYPES_H

typedef unsigned char      u8;
typedef signed   char      i8;
typedef unsigned short     u16;
typedef signed   short     i16;
typedef unsigned int       u32;
typedef signed   int       i32;
typedef unsigned long long u64;
typedef signed   long long i64;
typedef u32 usize;
typedef u32 uintptr;

#endif /* LINDOWS_TYPES_H */
