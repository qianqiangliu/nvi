/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 9.1 1994/11/09 18:36:35 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:36:35 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_yank -- [buffer][count]Y
 * 	     [buffer][count]y[count][motion]
 *	Yank text (or lines of text) into a cut buffer.
 *
 * !!!
 * Historic vi moved the cursor to the from MARK if it was before the current
 * cursor and on a different line, e.g., "yj" moves the cursor but "yk" and
 * "yh" do not.  Unfortunately, it's too late to change this now.  Matching
 * the historic semantics isn't easy.  The line number was always changed and
 * column movement was usually relative.  However, "y'a" moved the cursor to
 * the first non-blank of the line marked by a, while "y`a" moved the cursor
 * to the line and column marked by a.  Hopefully, the motion component code
 * got it right...   Unlike delete, we make no adjustments here.
 */
int
v_yank(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	int lmode;

	/* The line may not exist in line mode cuts, check to be sure. */
	if (F_ISSET(vp, VM_LMODE)) {
		if (file_gline(sp, vp->m_stop.lno, NULL) == NULL) {
			v_eof(sp, &vp->m_start);
			return (1);
		}
		lmode = CUT_LINEMODE;
	} else
		lmode = 0;
	if (cut(sp,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, lmode))
		return (1);

	sp->rptlines[L_YANKED] += (vp->m_stop.lno - vp->m_start.lno) + 1;
	return (0);
}
