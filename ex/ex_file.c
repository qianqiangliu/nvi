/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.21 1993/04/05 07:11:34 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:34 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

int
ex_file(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	char *p;

	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if ((p = strdup((char *)cmdp->argv[0])) == NULL) {
			msgq(sp, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		if (F_ISSET(ep, F_NONAME))
			F_CLR(ep, F_NONAME);
		else
			free(ep->name);
		ep->name = p;
		F_SET(ep, F_NAMECHANGED);
		break;
	default:
		abort();
	}
	status(sp, ep, cmdp->addr1.lno);
	return (0);
}
