/* $Id: acconfig.h,v 8.6 1996/02/25 18:23:18 bostic Exp $ (Berkeley) $Date: 1996/02/25 18:23:18 $ */

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define if you have a BSD version of curses. */
#undef HAVE_BSD_CURSES

/* Define if you have the curses(3) addnstr function. */
#undef HAVE_CURSES_ADDNSTR

/* Define if you have the curses(3) beep function. */
#undef HAVE_CURSES_BEEP

/* Define if you have the curses(3) flash function. */
#undef HAVE_CURSES_FLASH

/* Define if you have the curses(3) idlok function. */
#undef HAVE_CURSES_IDLOK

/* Define if you have the curses(3) keypad function. */
#undef HAVE_CURSES_KEYPAD

/* Define if you have the curses(3) newterm function. */
#undef HAVE_CURSES_NEWTERM

/* Define if you have the curses(3) setupterm function. */
#undef HAVE_CURSES_SETUPTERM

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#undef HAVE_CURSES_TIGETSTR

/* Define if you have the DB __hash_open call in the C library. */
#undef HAVE_DB_HASH_OPEN

/* Define if you have the chsize(2) system call.
#undef HAVE_FTRUNCATE_CHSIZE

/* Define if you have the ftruncate(2) system call.
#undef HAVE_FTRUNCATE_FTRUNCATE

/* Define if you have fcntl(2) style locking. */
#undef HAVE_LOCK_FCNTL

/* Define if you have flock(2) style locking. */
#undef HAVE_LOCK_FLOCK

/* Define if you have the Berkeley style revoke(2) system call. */
#undef HAVE_REVOKE

/* Define if you have a <sys/select.h> include file. */
#undef HAVE_SYS_SELECT_H

/* Define if you have the System V style pty calls. */
#undef HAVE_SYS5_PTY

/* Define if you have the libraries to support a Tcl interpreter. */
#undef HAVE_TCL_INTERP

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT

/* @BOTTOM@ */

#include "port.h"