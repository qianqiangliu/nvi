/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.19 1993/04/05 07:11:29 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:29 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_delete(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cut(sp, ep, cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	delete(sp, ep, &cmdp->addr1, &cmdp->addr2, 1);

	/*
	 * Deletion sets the cursor to the line after the last line deleted,
	 * or the last line in the file if delete to the end of the file.
	 */
	sp->lno = cmdp->addr2.lno;
	if (sp->lno > file_lline(sp, ep))
		sp->lno = file_lline(sp, ep);
	sp->cno = 0;

	/* Set autoprint. */
	F_SET(sp, S_AUTOPRINT);

	return (0);
}
