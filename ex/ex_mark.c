/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 10.7 1996/02/26 14:22:32 bostic Exp $ (Berkeley) $Date: 1996/02/26 14:22:32 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_mark -- :mark char
 *	      :k char
 *	Mark lines.
 *
 *
 * PUBLIC: int ex_mark __P((SCR *, EXCMD *));
 */
int
ex_mark(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	NEEDFILE(sp, cmdp);

	if (cmdp->argv[0]->len != 1) {
		msgq(sp, M_ERR, "136|Mark names must be a single character");
		return (1);
	}
	return (mark_set(sp, cmdp->argv[0]->bp[0], &cmdp->addr1, 1));
}
