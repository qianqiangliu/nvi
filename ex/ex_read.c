/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.31 1993/04/05 07:11:44 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_read --	:read[!] [file]
 *		:read [! cmd]
 *	Read from a file or utility.
 */
int
ex_read(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register char *p;
	struct stat sb;
	FILE *fp;
	MARK rm;
	int force;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->string) == NULL) {
		if (F_ISSET(ep, F_NONAME)) {
			msgq(sp, M_ERROR, "No filename from which to read.");
			return (1);
		}
		fname = ep->name;
		force = 0;
		goto noargs;
	}

	/* If "read!" it's a force from a file. */
	if (*p == '!') {
		force = 1;
		++p;
	} else
		force = 0;

	/* Skip whitespace. */
	for (; *p && isspace(*p); ++p);

	/* If "read !" it's a pipe from a utility. */
	if (*p == '!') {
		for (; *p && isspace(*p); ++p);
		if (*p == '\0') {
			msgq(sp, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		if (filtercmd(sp, ep, &cmdp->addr1, NULL, &rm, ++p, NOINPUT))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* Build an argv. */
	if (buildargv(sp, ep, p, 1, &cmdp->argc, &cmdp->argv))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = ep->name;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		break;
	default:
		msgq(sp, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* Open the file. */
noargs:	if ((fp = fopen(fname, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/* If not a regular file, force must be set. */
	if (!force && !S_ISREG(sb.st_mode)) {
		msgq(sp, M_ERROR,
		    "%s is not a regular file -- use ! to read it.", fname);
		return (1);
	}

	if (ex_readfp(sp, ep, fname, fp, &cmdp->addr1, &sp->rptlines))
		return (1);

	/* Set the cursor. */
	sp->lno = cmdp->addr1.lno + 1;
	sp->cno = 0;
	
	/* Set autoprint. */
	F_SET(sp, S_AUTOPRINT);

	/* Set reporting. */
	sp->rptlabel = "read";
	return (0);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 */
int
ex_readfp(sp, ep, fname, fp, fm, cntp)
	SCR *sp;
	EXF *ep;
	char *fname;
	FILE *fp;
	MARK *fm;
	recno_t *cntp;
{
	size_t len;
	recno_t lno;
	int rval;
	char *p;

	/*
	 * There is one very nasty special case.  The historic vi code displays
	 * a single space (or a '$' if the list option is set) for the first
	 * line in an "empty" file.  If we "insert" a line, that line gets
	 * scrolled down, not repainted, so it's incorrect when we refresh the
	 * the screen.  This is really hard to find and fix in the vi code --
	 * the text input functions detect it explicitly and don't insert a new
	 * line.  The hack here is to repaint the screen if we're appending to
	 * an empty file.
	 */
	if (file_lline(sp, ep) == 0)
		F_SET(sp, S_REDRAW);

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	rval = 0;
	for (lno = fm->lno; p = ex_getline(sp, fp, &len); ++lno)
		if (file_aline(sp, ep, lno, p, len)) {
			rval = 1;
			break;
		}

	if (ferror(fp)) {
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		rval = 1;
	}

	if (fclose(fp)) {
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = lno - fm->lno;

	return (0);
}
