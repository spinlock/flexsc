/* Copyright (C) 1991, 92, 93, 96, 98, 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <sysdeps/generic/sysdep.h>

#include <sys/syscall.h>
#define	HAVE_SYSCALLS

/* Note that using a `PASTE' macro loses.  */
#ifdef	__STDC__
#define	SYSCALL__(name, args)	PSEUDO (__##name, name, args)
#else
#define	SYSCALL__(name, args)	PSEUDO (__/**/name, name, args)
#endif
#define	SYSCALL(name, args)	PSEUDO (name, name, args)

/* Machine-dependent sysdep.h files are expected to define the macro
   PSEUDO (function_name, syscall_name) to emit assembly code to define the
   C-callable function FUNCTION_NAME to do system call SYSCALL_NAME.
   r0 and r1 are the system call outputs.  MOVE(x, y) should be defined as
   an instruction such that "MOVE(r1, r0)" works.  ret should be defined
   as the return instruction.  */

#ifdef __STDC__
#define SYS_ify(syscall_name) SYS_##syscall_name
#else
#define SYS_ify(syscall_name) SYS_/**/syscall_name
#endif

/* Terminate a system call named SYM.  This is used on some platforms
   to generate correct debugging information.  */
#ifndef PSEUDO_END
#define PSEUDO_END(sym)
#endif
#ifndef PSEUDO_END_NOERRNO
#define PSEUDO_END_NOERRNO(sym)	PSEUDO_END(sym)
#endif
#ifndef PSEUDO_END_ERRVAL
#define PSEUDO_END_ERRVAL(sym)	PSEUDO_END(sym)
#endif

/* Wrappers around system calls should normally inline the system call code.
   But sometimes it is not possible or implemented and we use this code.  */
/* #define INLINE_SYSCALL(name, nr, args...) __syscall_##name (args) */

#ifndef __FLEXSC_LOAD_X
#define __FLEXSC_LOAD_X
#  define __FLEXSC_LOAD_ARGS_0()
#  define __FLEXSC_LOAD_ARGS_1(a0)              \
    __FLEXSC_LOAD_ARGS_0();                     \
    __sysargs[0] = (long)(a0);
#  define __FLEXSC_LOAD_ARGS_2(a0, a1)          \
    __FLEXSC_LOAD_ARGS_1(a0);                   \
    __sysargs[1] = (long)(a1);
#  define __FLEXSC_LOAD_ARGS_3(a0, a1, a2)      \
    __FLEXSC_LOAD_ARGS_2(a0, a1);               \
    __sysargs[2] = (long)(a2);
#  define __FLEXSC_LOAD_ARGS_4(a0, a1, a2, a3)  \
    __FLEXSC_LOAD_ARGS_3(a0, a1, a2);           \
    __sysargs[3] = (long)(a3);
#  define __FLEXSC_LOAD_ARGS_5(a0, a1, a2, a3, a4)  \
    __FLEXSC_LOAD_ARGS_4(a0, a1, a2, a3);           \
    __sysargs[4] = (long)(a4);
#  define __FLEXSC_LOAD_ARGS_6(a0, a1, a2, a3, a4, a5)  \
    __FLEXSC_LOAD_ARGS_5(a0, a1, a2, a3, a4);           \
    __sysargs[5] = (long)(a5);
#endif /* !__FLEXSC_LOAD_X */

#define INLINE_SYSCALL(name, nr, args...) ({                                                \
            extern long (*__flexsc_syscall_handle)(long sysargs[], unsigned int sysname);   \
            long __sysargs[8];                                                              \
            __FLEXSC_LOAD_ARGS_##nr(args);                                                  \
            __sysargs[6] = (unsigned int)(name);                                            \
            __flexsc_syscall_handle(__sysargs, name);                                       \
        })
