/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_scroll.c,v 5.6 1992/10/01 17:30:50 bostic Exp $ (Berkeley) $Date: 1992/10/01 17:30:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <curses.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "screen.h"
#include "extern.h"

#define	DOWN(lno) {							\
	if (file_gline(curf, lno, &len) == NULL) {			\
		v_eof(fm);						\
		return (1);						\
	}								\
	rp->lno = lno;							\
	rp->cno = len ? fm->cno > len - 1 ? len - 1 : fm->cno : 0;	\
}

/*
 * v_lgoto -- [count]G
 *	Go to first non-blank character of the line count, the last line
 *	of the file by default.
 */
int
v_lgoto(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (vp->flags & VC_C1SET) {
		if (file_gline(curf, vp->count, NULL) == NULL) {
			bell();
			if (ISSET(O_VERBOSE))
				msg("Line %lu doesn't exist.", vp->count);
			return (1);
		}
		rp->lno = vp->count;
	} else
		rp->lno = file_lline(curf);
	return (v_nonblank(rp));
}

/* 
 * v_home -- [count]H
 *	Move to the first non-blank character of the line count from
 *	the top of the screen, 1 by default.
 */
int
v_home(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_long lno;

	lno = curf->top + (vp->flags & VC_C1SET ? vp->count : 0);

	DOWN(lno);
	return (v_nonblank(rp));
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the line in the middle
 *	of the screen.
 */
int
v_middle(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long lno;

	if (file_gline(curf, BOTLINE, NULL) == NULL) {
		lno = file_lline(curf) / 2;
		if (lno == 0)
			lno = 1;
		rp->lno = lno;
	} else
		rp->lno = curf->top + LINES / 2;
	return (v_nonblank(rp));
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the line count from
 *	the bottom of the screen, 1 by default.
 */
int
v_bottom(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;

	if (file_gline(curf, BOTLINE, NULL) == NULL) {
		lno = file_lline(curf) / 2;
		if (lno == 0)
			lno = 1;
	} else
		lno = BOTLINE;

	cnt = vp->flags & VC_C1SET ? vp->count : 0;
	if (cnt >= lno) {
		v_sof(fm);
		return (1);
	}
	rp->lno = lno - cnt;
	return (v_nonblank(rp));
}

/*
 * v_nbdown -- [count]^M, [count]+
 *	Move down by lines, moving cursor to first non-blank character.
 */
int
v_nbdown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_long lno;

	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1);

	DOWN(lno);
	return (v_nonblank(rp));
}

/*
 * v_down -- [count]^J, [count]^N, [count]j
 *	Move down by lines.
 */
int
v_down(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_long lno;

	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1);

	DOWN(lno);
	return (0);
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long lno;

	/* 
	 * Half screens set the scroll value even if the command ultimately
	 * failed in historic vi.  It's probably a don't care.
	 */
	if (vp->flags & VC_C1SET)
		LVAL(O_SCROLL) = vp->count;
	else
		vp->count = LVAL(O_SCROLL);

	/* Half screens always succeed unless at the bottom of the file. */
	lno = fm->lno + vp->count;

	if (file_gline(curf, lno, NULL) == NULL) {
		lno = file_lline(curf);
		if (lno == fm->lno) {
			v_eof(fm);
			return (1);
		}
	}
	rp->lno = lno;
	return (v_nonblank(rp));
}

/*
 * v_pagedown -- [count]^F
 *	Page down by screens.
 */
int
v_pagedown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_long lno;

	/* Calculation from POSIX 1003.2/D8. */
	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1) * (LINES - 2);

	DOWN(lno);
	return (v_nonblank(rp));
}

/*
 * v_lineup -- [count]^E
 *	Page up by lines.
 */
int
v_linedown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long off;

	off = vp->flags & VC_C1SET ? vp->count : 1;

	/* Can't go past the end of the file. */
	if (file_gline(curf, BOTLINE + off, NULL) == NULL) {
		off = file_lline(curf) - BOTLINE;
		if (off == 0) {
			v_eof(fm);
			return (1);
		}
	}

	/* Set the number of lines to scroll. */
	curf->uwindow = off;

	/*
	 * The cursor moves up, staying with its original line,
	 * unless it reaches the top of the screen.
	 */
	rp->lno = fm->lno == curf->top ? fm->lno + off : fm->lno;
	rp->cno = fm->cno;
	return (0);
}
