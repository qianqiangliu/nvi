/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 5.5 1992/05/07 12:49:38 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>
#include <ctype.h>

#include "vi.h"
#include "exf.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ulcase -- ~
 *	This function toggles upper & lower case letters.
 */
MARK *
v_ulcase(m, cnt)
	MARK *m;			/* Where to make the change. */
	register long cnt;	/* Number of chars to flip. */
{
	MARK rval;
	register char ch, *from, *to;
	long scnt;
	int madechange;
	char lbuf[1024];

	SETDEFCNT(1);

	/* Fetch the current version of the line. */
	from = file_line(curf, m->lno, NULL);

	scnt = cnt;
	madechange = 0;
	for (from += m->cno, to = lbuf; cnt--; to)  {
		if ((ch = *from++) == '\0')
			break;
		if (isupper(ch)) {
			*to++ = tolower(ch);
			madechange = 1;
		} else if (islower(ch)) {
			*to++ = toupper(ch);
			madechange = 1;
		} else
			*to++ = ch;
	}

	rval.lno = m->lno;
	rval.cno = m->cno + scnt;
	if (madechange) {
		lbuf[scnt] = '\0';
		change(m, &rval, lbuf, scnt);
	}
	return (&rval);
}
