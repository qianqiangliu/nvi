/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tcl.c,v 8.5 1996/02/26 14:22:42 bostic Exp $ (Berkeley) $Date: 1996/02/26 14:22:42 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

#ifdef HAVE_TCL_INTERP
#include <tcl.h>
#endif

/* 
 * ex_tcl -- :[line [,line]] tcl [command]
 *	Run a command through the tcl interpreter.
 *
 * PUBLIC: int ex_tcl __P((SCR*, EXCMD *));
 */
int 
ex_tcl(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
#ifdef HAVE_TCL_INTERP
	CHAR_T *p;
	GS *gp;
	size_t len;
	char buf[64];

	/* Skip leading white space. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len; len > 0; --len, ++p)
			if (!isblank(*p))
				break;
	if (cmdp->argc == 0 || len == 0) {
		ex_emsg(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	gp = sp->gp;
	(void)snprintf(buf, sizeof(buf),
	    "set viScreenId %d\nset viStartLine %lu\nset viStopLine %lu",
	    sp->id, cmdp->addr1.lno, cmdp->addr2.lno);
	if (Tcl_Eval(gp->interp, buf) == TCL_OK &&
	    Tcl_Eval(gp->interp, cmdp->argv[0]->bp) == TCL_OK)
		return (0);

	msgq(sp, M_ERR, "Tcl: %s", ((Tcl_Interp *)gp->interp)->result);
	return (1);
#else
	msgq(sp, M_ERR, "302|Vi was not loaded with a Tcl interpreter");
	return (1);
#endif /* HAVE_TCL_INTERP */
}
