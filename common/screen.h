/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 10.4 1995/07/06 11:51:01 bostic Exp $ (Berkeley) $Date: 1995/07/06 11:51:01 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  The row
 * minimum is fixed at 1 (the svi code can share a line between the text line
 * and the colon command/message line).  Column calculation is a lot trickier.
 * For example, you have to have enough columns to display the line number,
 * not to mention guaranteeing that tabstop and shiftwidth values are smaller
 * than the current column value.  It's simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is almost certainly wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 1
#define	MINIMUM_SCREEN_COLS	20

/*
 * Loops check for interrupts after every INTERRUPT_CHECK lines.  The value
 * wasn't chosen for any terribly good reason, but it seems to work okay.
 * The idea is to find a balance between getting work done and shutting out
 * the event handler.
 *
 * That said, let's talk about the event driven-ness of nvi for a second.  I
 * put some *serious* effort into making nvi fully event driven and fully
 * interruptible.  Ask me for the horror stories, but the bottom line is that
 * it can't be done without having language or system support, e.g. threads.
 * The current status is:
 *
 *	Command loops are event-driven, with the single caveat that nvi must
 *	be able to get a single keyboard event by a callback into the screen
 *	code when prompting to see if the user has seen a screen display.
 *	See v_getkey() for more information.
 *
 *	Command loops are interruptible, as long as the application supports
 *	asynchronous events or can check the event queue as part of a callback
 *	function.
 *
 * It would have been nice to return to the event code periodically for an
 * interrupt check, but it's just too damned hard.
 */
#define	INTERRUPT_CHECK	200
#define	INTERRUPTED(sp)							\
	(F_ISSET(sp, S_INTERRUPTED) ||					\
	(sp)->gp->scr_interrupt != NULL && (sp)->gp->scr_interrupt(sp))

/*
 * SCR --
 *	The screen structure.  To the extent possible, all screen information
 *	is stored in the various private areas.  The only information here
 *	is used by global routines or is shared by too many screens.
 */
struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	CIRCLEQ_ENTRY(_scr) q;		/* Screens. */

	int	 refcnt;		/* Reference count. */

	GS	*gp;			/* Pointer to global area. */
	SCR	*nextdisp;		/* Next display screen. */
	EXF	*ep;			/* Screen's current EXF structure. */

	FREF	*frp;			/* FREF being edited. */
	char	**argv;			/* NULL terminated file name array. */
	char	**cargv;		/* Current file name. */

	u_long	 ccnt;			/* Command count. */
	u_long	 q_ccnt;		/* Quit or ZZ command count. */

					/* Screen's: */
	size_t	 rows;			/* 1-N: number of rows. */
	size_t	 cols;			/* 1-N: number of columns. */
	size_t	 t_rows;		/* 1-N: cur number of text rows. */
	size_t	 t_maxrows;		/* 1-N: max number of text rows. */
	size_t	 t_minrows;		/* 1-N: min number of text rows. */
	size_t	 woff;			/* 0-N: screen offset in frame. */

					/* Cursor's: */
	recno_t	 lno;			/* 1-N: file line. */
	size_t	 cno;			/* 0-N: file character in line. */

	size_t	 rcm;			/* Vi: 0-N: Most attractive column. */

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_DELETED	2		/* Deleted lines. */
#define	L_JOINED	3		/* Joined lines. */
#define	L_MOVED		4		/* Moved lines. */
#define	L_SHIFT		5		/* Shift lines. */
#define	L_YANKED	6		/* Yanked lines. */
	recno_t	 rptlchange;		/* Ex/vi: last L_CHANGED lno. */
	recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	FILE	*stdfp;			/* Ex output file pointer. */

	char	*if_name;		/* Ex input file name, for messages. */
	recno_t	 if_lno;		/* Ex input file line, for messages. */

	fd_set	 rdfd;			/* Ex/vi: read fd select mask. */

	TEXTH	 tiq;			/* Vi: text input queue. */

	SCRIPT	*script;		/* Vi: script mode information .*/

	recno_t	 defscroll;		/* Vi: ^D, ^U scroll information. */

					/* Display character. */
	CHAR_T	 cname[MAX_CHARACTER_COLUMNS + 1];
	size_t	 clen;			/* Length of display character. */

	enum {				/* Vi editor mode. */
	    SM_APPEND = 0, SM_CHANGE, SM_COMMAND, SM_INSERT,
	    SM_REPLACE } showmode;

	void	*ex_private;		/* Ex private area. */
	void	*sex_private;		/* Ex screen private area. */
	void	*vi_private;		/* Vi private area. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	char	*alt_name;		/* Ex/vi: alternate file name. */

	CHAR_T	   at_lbuf;		/* Ex/vi: Last executed at buffer. */

					/* Ex/vi: search/substitute info. */
	dir_t	  searchdir;		/* Last file search direction. */
	regex_t	  subre;		/* Last subst. RE (compiled form). */
	regex_t	  sre;			/* Last search RE (compiled form). */
	char	 *re;			/* Last search RE (uncompiled form). */
	size_t	  re_len;		/* Last search RE length. */
	char	 *repl;			/* Substitute replacement. */
	size_t	  repl_len;		/* Substitute replacement length.*/
	size_t	 *newl;			/* Newline offset array. */
	size_t	  newl_len;		/* Newline array size. */
	size_t	  newl_cnt;		/* Newlines in replacement. */
	u_int8_t  c_suffix;		/* Edcompatible 'c' suffix value. */
	u_int8_t  g_suffix;		/* Edcompatible 'g' suffix value. */

#define	RE_WSTART	"[[:<:]]"	/* Not-in-word search patterns. */
#define	RE_WSTOP	"[[:>:]]"

#define	SEARCH_EOL	0x0001		/* Offset past EOL is okay. */
#define	SEARCH_FILE	0x0002		/* Search the entire file. */
#define	SEARCH_MSG	0x0004		/* Display search warning messages. */
#define	SEARCH_PARSE	0x0008		/* Parse the search pattern. */
#define	SEARCH_SET	0x0010		/* Set search direction. */
#define	SEARCH_TAG	0x0020		/* Search pattern is a tag pattern. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Options. */

/*
 * Screen flags.
 *
 * Editor screens.
 */
#define	S_EX		0x00000001	/* Ex screen. */
#define	S_VI		0x00000002	/* Vi screen. */

/*
 * Screen formatting flags, first major, then minor.
 *
 * S_SCR_REFORMAT
 *	The expected presentation of the lines on the screen have changed,
 *	requiring that the intended screen lines be recalculated.  Implies
 *	S_SCR_REDRAW.
 * S_SCR_REDRAW
 *	The screen doesn't correctly represent the file; repaint it.  Note,
 *	setting S_SCR_REDRAW in the current window causes *all* windows to
 *	be repainted.
 *
 * S_SCR_CENTER
 *	If the current line isn't already on the screen, center it.
 * S_SCR_TOP
 *	If the current line isn't already on the screen, put it at the top.
 */
#define	S_SCR_REFORMAT	0x00000004	/* Reformat (refresh). */
#define	S_SCR_REDRAW	0x00000008	/* Refresh. */

#define	S_SCR_CENTER	0x00000010	/* Center the line if not visible. */
#define	S_SCR_TOP	0x00000020	/* Top the line if not visible. */

/* Screen/file changes. */
#define	S_EXIT		0x00000040	/* Exiting (not forced). */
#define	S_EXIT_FORCE	0x00000080	/* Exiting (forced). */
#define	S_SSWITCH	0x00000100	/* Switch screens. */

#define	S_ARGNOFREE	0x00000200	/* Argument list wasn't allocated. */
#define	S_ARGRECOVER	0x00000400	/* Argument list is recovery files. */
#define	S_AT_SET	0x00000800	/* Last at buffer set. */
#define	S_COMPLETE	0x00001000	/* Command completed. */
#define	S_COMPLETE_EX	0x00002000	/* Vi: ex command completed. */
#define	S_EX_CANON	0x00004000      /* Ex: tty is in canonical mode. */
#define	S_EX_GLOBAL	0x00008000	/* Ex: in the global command. */
#define	S_EX_SILENT	0x00010000	/* Ex: batch script. */
#define	S_INPUT		0x00020000	/* Doing text input. */
#define	S_INPUT_INFO	0x00040000	/* Doing text input on info line. */
#define	S_INTERRUPTED	0x00080000	/* An interrupt occurred. */
#define	S_RE_RECOMPILE	0x00100000	/* The search RE needs recompiling. */
#define	S_RE_SEARCH	0x00200000	/* The search RE has been set. */
#define	S_RE_SUBST	0x00400000	/* The substitute RE has been set. */
#define	S_SCRIPT	0x00800000	/* Window is a shell script. */
#define	S_STATUS	0x01000000	/* Schedule welcome message. */
	u_int32_t flags;
};
