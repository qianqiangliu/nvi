/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 5.11 1993/05/06 00:37:43 bostic Exp $ (Berkeley) $Date: 1993/05/06 00:37:43 $";
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>

#include "vi.h"
#include "excmd.h"

static void db __P((SCR *, char *, CB *));

/*
 * ex_bdisplay -- :bdisplay
 *	Display cut buffer contents.
 */
int
ex_bdisplay(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cb;
	int cnt;

	for (cb = sp->cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (cb->txthdr.next == NULL || cb->txthdr.next == &cb->txthdr)
			continue;
		db(sp, charname(sp, cnt), cb);
	}
	if (sp->cuts[DEFCB].txthdr.next != NULL ||
	    sp->cuts[DEFCB].txthdr.next != &sp->cuts[DEFCB].txthdr)
		db(sp, "default buffer", &sp->cuts[DEFCB]);
	return (0);
}

static void
db(sp, name, cb)
	SCR *sp;
	char *name;
	CB *cb;
{
	TEXT *tp;

	(void)fprintf(sp->stdfp, "%s%s\n", name,
	    F_ISSET(cb, CB_LMODE) ? ": line mode" : "");
	for (tp = cb->txthdr.next; tp != (TEXT *)&cb->txthdr; tp = tp->next)
		(void)fprintf(sp->stdfp, "%.*s\n", (int)tp->len, tp->lb);
}
