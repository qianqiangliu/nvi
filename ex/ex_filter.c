/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_filter.c,v 8.4 1993/09/08 14:51:56 bostic Exp $ (Berkeley) $Date: 1993/09/08 14:51:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static int	ldisplay __P((SCR *, FILE *));
static int	filter_wait __P((SCR *, pid_t, char *, int));

/*
 * filtercmd --
 *	Run a range of lines through a filter utility and optionally
 *	replace the original text with the stdout/stderr output of
 *	the utility.
 */
int
filtercmd(sp, ep, fm, tm, rp, cmd, ftype)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm, *rp;
	char *cmd;
	enum filtertype ftype;
{
	FILE *ifp, *ofp;		/* GCC: can't be uninitialized. */
	pid_t parent_writer_pid, utility_pid;
	recno_t lno, nread;
	int input[2], output[2], rval;
	char *name;

	/*
	 * There are three different processes running through this code.
	 * They are the utility, the parent-writer and the parent-reader.
	 * The parent-writer is the process that writes from the file to
	 * the utility, the parent reader is the process that reads from
	 * the utility.
	 *
	 * Input and output are named from the utility's point of view.
	 */
	input[0] = input[1] = output[0] = output[1] = -1;

	/*
	 * Set default return cursor position; guard against a line number
	 * of zero.
	 */
	*rp = *fm;
	if (fm->lno == 0)
		rp->lno = 1;

	/*
	 * In the FILTER_READ case, the utility isn't expected to want
	 * input.  Redirect its input from /dev/null.  Otherwise open
	 * up utility input pipe.
	 */
	if (ftype == FILTER_READ) {
		if ((input[0] = open(_PATH_DEVNULL, O_RDONLY, 0)) < 0) {
			msgq(sp, M_ERR,
			    "filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			goto err1;
		}
	} else
		if (pipe(input) < 0 || (ifp = fdopen(input[1], "w")) == NULL)
			goto err2;

	/* Open up utility output pipe. */
	if (pipe(output) < 0 || (ofp = fdopen(output[0], "r")) == NULL)
		goto err2;

	/* Fork off the utility process. */
	switch (utility_pid = vfork()) {
	case -1:			/* Error. */
err2:		msgq(sp, M_ERR, "filter: %s", strerror(errno));
err1:		if (input[0] != -1)
			(void)close(input[0]);
		if (input[1] != -1)
			(void)close(input[1]);
		if (output[0] != -1)
			(void)close(output[0]);
		if (output[1] != -1)
			(void)close(input[1]);
		return (1);
	case 0:				/* Utility. */
		/*
		 * Redirect stdin from the read end of the input pipe,
		 * and redirect stdout/stderr to the write end of the
		 * output pipe.
		 */
		(void)dup2(input[0], STDIN_FILENO);
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the child's pipe file descriptors. */
		(void)close(input[0]);
		if (ftype != FILTER_READ)
			(void)close(input[1]);
		if (ftype != FILTER_WRITE)
			(void)close(output[0]);
		(void)close(output[1]);

		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;
		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		msgq(sp, M_ERR,
		    "exec: %s: %s", O_STR(sp, O_SHELL), strerror(errno));
		_exit (1);
		/* NOTREACHED */
	default:			/* Parent-reader, parent-writer. */
		/* Close the pipe ends neither parent will use. */
		(void)close(input[0]);
		(void)close(output[1]);
		break;
	}

	/*
	 * FILTER_READ:
	 *
	 * Reading is the simple case -- we don't need a parent writer, so
	 * the parent reads the output from the read end of the output pipe
	 * until it finishes, then waits for the child.  Ex_readfp appends
	 * to the MARK, and closes ofp.  Set the return cursor to the last
	 * line read in, checking to make sure that it's not past EOF because
	 * we were reading into an empty file.
	 */
	if (ftype == FILTER_READ) {
		rval = ex_readfp(sp, ep, "filter", ofp, fm, &nread, 0);
		sp->rptlines[L_ADDED] += nread;
		if (fm->lno == 0)
			rp->lno = nread;
		else
			rp->lno += nread;
		return (rval | filter_wait(sp, utility_pid, cmd, 0));
	}

	/*
	 * FILTER, FILTER_WRITE
	 *
	 * Here we need both a reader and a writer.  Temporary files are
	 * expensive and we'd like to avoid disk I/O.  Using pipes has the
	 * obvious starvation conditions.  It's done as follows:
	 *
	 *	fork
	 *	child
	 *		write lines out
	 *		exit
	 *	parent
	 *		FILTER:
	 *			read lines into the file
	 *			delete old lines
	 *		FILTER_WRITE
	 *			read and display lines
	 *		wait for child
	 *
	 * XXX
	 * We get away without locking the underlying database because we know
	 * that none of the records that we're reading will be modified until
	 * after we've read them.  This depends on the fact that the current
	 * B+tree implementation doesn't balance pages or similar things when
	 * it inserts new records.  When the DB code has locking, we should
	 * treat vi as if it were multiple applications sharing a database, and
	 * do the required locking.  If necessary a work-around would be to do
	 * explicit locking in the line.c:file_gline() code, based on the flag
	 * set here.
	 */
	rval = 0;
	F_SET(ep, F_MULTILOCK);
	switch (parent_writer_pid = fork()) {
	case -1:			/* Error. */
		msgq(sp, M_ERR, "filter: %s", strerror(errno));
		(void)close(input[1]);
		(void)close(output[0]);
		(void)filter_wait(sp, utility_pid, cmd, 0);
		F_CLR(ep, F_MULTILOCK);
		return (1);
	case 0:				/* Parent-writer. */
		/*
		 * Write the selected lines to the write end of the
		 * input pipe.  Ifp is closed by ex_writefp.
		 */
		(void)close(output[0]);
		_exit(ex_writefp(sp, ep, "filter", ifp, fm, tm, 0));

		/* NOTREACHED */
	default:			/* Parent-reader. */
		(void)close(input[1]);
		if (ftype == FILTER_WRITE)
			/*
			 * Read the output from the read end of the output
			 * pipe and display it.  Ldisplay closes ofp.
			 */
			rval = ldisplay(sp, ofp);
		else {
			/*
			 * Read the output from the read end of the output
			 * pipe.  Ex_readfp appends to the MARK and closes
			 * ofp.
			 */
			rval = ex_readfp(sp, ep, "filter", ofp, tm, &nread, 0);
			sp->rptlines[L_ADDED] += nread;

			/* Delete any lines written to the utility. */
			if (rval == 0) {
				for (lno = tm->lno; lno >= fm->lno; --lno)
					if (file_dline(sp, ep, lno)) {
						rval = 1;
						break;
					}
				if (rval == 0)
					sp->rptlines[L_DELETED] +=
					    (tm->lno - fm->lno) + 1;
			}
		}
		rval |= filter_wait(sp, parent_writer_pid, "parent-writer", 1);
		break;
	}
	F_CLR(ep, F_MULTILOCK);
	return (rval | filter_wait(sp, utility_pid, cmd, 0));
}

/*
 * filter_wait --
 *	Wait for one of the filter processes.
 */
int
filter_wait(sp, pid, cmd, okpipe)
	SCR *sp;
	pid_t pid;
	char *cmd;
	int okpipe;
{
	sig_ret_t intsave, quitsave;
	size_t len;
	int pstat;

	/* Save the parent's signal values. */
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);

	/* Wait for the utility to finish. */
	(void)waitpid(pid, &pstat, 0);

	/* Restore the parent's signal masks. */
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);

	/*
	 * Display the utility's exit status.  Ignore SIGPIPE from the
	 * parent-writer, as that only means that the utility chose to
	 * exit before reading all of its input.
	 */
	if (WIFSIGNALED(pstat) && (!okpipe || WTERMSIG(pstat) != SIGPIPE)) {
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: received signal: %s%s.",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    sys_siglist[WTERMSIG(pstat)],
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		return (1);
	}
	if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: exited with status %d",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    WEXITSTATUS(pstat));
		return (1);
	}
	return (0);
}

/*
 * ldisplay --
 *	Display a line output from a utility.
 *
 * XXX
 * This should probably be combined with some of the ex_print() routines
 * into a single display routine.
 */
static int
ldisplay(sp, fp)
	SCR *sp;
	FILE *fp;
{
	CHNAME *cname;
	char *p;
	size_t len;
	int rval;

	cname = sp->cname;
	for (rval = 0; !ex_getline(sp, fp, &len);) {
		for (p = sp->ibp; len--; ++p) {
			(void)fprintf(sp->stdfp, "%s", cname[*p].name);
			if (ferror(sp->stdfp)) {
				msgq(sp, M_ERR,
				    "I/O error: %s", strerror(errno));
				(void)fclose(fp);
				return (1);
			}
		}
		putc('\n', sp->stdfp);
	}
	if (fclose(fp)) {
		msgq(sp, M_ERR, "I/O error: %s", strerror(errno));
		return (1);
	}
	return (0);
}
