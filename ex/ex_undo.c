/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.8 1992/11/11 18:29:32 bostic Exp $ (Berkeley) $Date: 1992/11/11 18:29:32 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "log.h"
#include "extern.h"

int
ex_undo(cmdp)
	EXCMDARG *cmdp;
{
	static enum { BACKWARD, FORWARD } last = FORWARD;
	MARK rp;

	autoprint = 1;

	switch(last) {
	case BACKWARD:
		last = FORWARD;
		if (log_forward(curf, &rp))
			return (1);
		break;
	case FORWARD:
		last = BACKWARD;
		if (log_backward(curf, &rp, OOBLNO))
			return (1);
		break;
	}
	curf->lno = rp.lno;
	curf->cno = rp.cno;
	return (0);
}
