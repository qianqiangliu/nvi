/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.15 1992/12/20 15:11:25 bostic Exp $ (Berkeley) $Date: 1992/12/20 15:11:25 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "log.h"
#include "options.h"
#include "screen.h"

#define	LOG_ERR {							\
	bell();								\
	msg("Error: %s/%d: retrieve log error: %s.",			\
	    tail(__FILE__), __LINE__, strerror(errno));			\
	goto err;							\
}

/*
 * v_Undo -- U
 *	Undo changes to this line, or roll forward.
 */
int
v_Undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (ISSET(O_NUNDO) ?
	    log_forward(curf, rp) : log_backward(curf, rp, curf->lno));
}
	
/*
 * v_undo -- u
 *	Undo the last change.
 */
int
v_undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	static enum { BACKWARD, FORWARD } last;

	if (ISSET(O_NUNDO))
		return (log_backward(curf, rp, OOBLNO));

	if (!FF_ISSET(curf, F_UNDO)) {
		last = FORWARD;
		FF_SET(curf, F_UNDO);
	}

	switch(last) {
	case BACKWARD:
		if (log_forward(curf, rp)) {
			FF_CLR(curf, F_UNDO);
			return (1);
		}
		last = FORWARD;
		break;
	case FORWARD:
		if (log_backward(curf, rp, OOBLNO)) {
			FF_CLR(curf, F_UNDO);
			return (1);
		}
		last = BACKWARD;
		break;
	}
	return (0);
}
