/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 5.5 1992/05/21 12:57:25 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:57:25 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

#include "vi.h"
#include "exf.h"
#include "mark.h"
#include "cut.h"
#include "vcmd.h"
#include "extern.h"

u_long atkeybuflen;				/* Length of shared buffer. */
char *atkeybuf, *atkeyp;			/* Shared at buffer. */

int
v_at(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	static char rstack[UCHAR_MAX];
	CB *cb;
	CBLINE *lp;
	size_t len, remain;
	int key;
	char *p, *start;

	key = vp->character;
	CBNAME(key, cb);
	CBEMPTY(key, cb);

	if (atkeybuflen == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[key]) {
		msg("Buffer %c already occurs in this command.", key);
		return (1);
	}

	/* Get buffer for rest of at string plus cut buffer. */
	remain = atkeybuflen ? atkeybuflen - (atkeyp - atkeybuf) : 0;

	/* Check for overflow. */
	len = cb->len + remain;
	if (len < cb->len + remain) {
		msg("Buffer overflow.");
		return (1);
	}

	if ((start = p = malloc(len)) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}

	/* Copy into the new buffer. */
	for (lp = cb->head;;) {
		bcopy(lp->lp, p, lp->len);
		p += lp->len;
		*p++ = '\n';
		if ((lp = lp->next) == NULL)
			break;
	}
	
	/* Copy the rest of the current at string into place. */
	if (atkeybuflen != 0) {
		bcopy(atkeyp, p, remain);
		free(atkeybuf);

	}
	/* Fix the pointers. */
	atkeybuf = atkeyp = start;
	atkeybuflen = len;

	rstack[key] = 1;

	return (0);
}
