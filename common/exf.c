/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 8.27 1993/09/29 16:15:03 bostic Exp $ (Berkeley) $Date: 1993/09/29 16:15:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "recover.h"

/*
 * file_add --
 *	Insert a file name into the FREF list, if it doesn't already
 *	appear in it.
 *
 * XXX
 * The "if they don't already appear" changes vi's semantics slightly.
 * If you do a "vi foo bar", and then execute "next bar baz", the edit
 * of bar will reflect the line/column of the previous edit session.
 * This seems reasonable to me, and is a logical extension of the change
 * where vi now remembers the last location in any file that it has ever
 * edited, not just the previously edited file.
 */
FREF *
file_add(sp, frp_append, fname, ignore)
	SCR *sp;
	FREF *frp_append;
	char *fname;
	int ignore;
{
	FREF *frp;

	/*
	 * Ignore if it already exists, with the exception that we turn off
	 * the ignore bit.  (The sequence is that the file was already part
	 * of an argument list, and had the ignore bit set as part of adding
	 * a new argument list.
	 */
	if (fname != NULL) {
		for (frp = sp->frefhdr.next;
		    frp != (FREF *)&sp->frefhdr; frp = frp->next)
			if (!strcmp(frp->fname, fname)) {
				F_CLR(frp, FR_IGNORE);
				return (frp);
			}
	}

	/* Allocate and initialize the FREF structure. */
	if ((frp = malloc(sizeof(FREF))) == NULL)
		goto mem;
	memset(frp, 0, sizeof(FREF));

	/*
	 * If no file name specified, set the appropriate flag.
	 *
	 * XXX
	 * This had better be *closely* followed by a file_init
	 * so something gets filled in!
	 */
	if (fname == NULL)
		F_SET(frp, FR_NONAME);
	else if ((frp->fname = strdup(fname)) == NULL) {
		FREE(frp, sizeof(FREF));
mem:		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (NULL);
	} else
		frp->nlen = strlen(fname);

	/* Only the initial argument list is "remembered". */
	if (ignore)
		F_SET(frp, FR_IGNORE);

	/* Append into the chain of file names. */
	if (frp_append != NULL) {
		HDR_APPEND(frp, frp_append, next, prev, FREF);
	} else
		HDR_INSERT(frp, &sp->frefhdr, next, prev, FREF);

	return (frp);
}

/*
 * file_first --
 *	Return the first file name for editing, if any.
 */
FREF *
file_first(sp, all)
	SCR *sp;
	int all;
{
	FREF *frp;

	/* Return the first file name. */
	for (frp = sp->frefhdr.next;
	    frp != (FREF *)&sp->frefhdr; frp = frp->next)
		if (all || !F_ISSET(frp, FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_next --
 *	Return the next file name, if any.
 */
FREF *
file_next(sp, all)
	SCR *sp;
	int all;
{
	FREF *frp;

	/* Return the next file name. */
	for (frp = sp->frefhdr.next;
	    frp != (FREF *)&sp->frefhdr; frp = frp->next)
		if (all || !F_ISSET(frp, FR_EDITED | FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_init --
 *	Start editing a file, based on the FREF structure.  If successsful,
 *	let go of any previous file.  Don't release the previous file until
 *	absolutely sure we have the new one.
 */
int
file_init(sp, frp, rcv_fname, force)
	SCR *sp;
	FREF *frp;
	char *rcv_fname;
	int force;
{
	EXF *ep;
	RECNOINFO oinfo;
	struct stat sb;
	size_t psize;
	int e_ep, e_tname, e_rcv_path, fd, sverrno;
	char *oname, tname[sizeof(_PATH_TMPNAME) + 1];

	/* Create the EXF. */
	if ((ep = malloc(sizeof(EXF))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	memset(ep, 0, sizeof(EXF));

	/* Allocate up to three pieces of memory; free on error. */
	e_ep = 1;
	e_tname = e_rcv_path = 0;

	/* Insert into the chain of EXF structures. */
	HDR_INSERT(ep, &sp->gp->exfhdr, next, prev, EXF);

	/* Set initial EXF flag bits. */
	F_SET(ep, F_FIRSTMODIFY);

	/*
	 * If no name or backing file, create a backing temporary file, saving
	 * the temp file name so can later unlink it.  Repoint the name to the
	 * temporary name (we display it to the user until they rename it).
	 */
	if (frp->fname == NULL || stat(frp->fname, &sb)) {
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_ERR, "Temporary file: %s", strerror(errno));
			goto err;
		}
		(void)close(fd);
		if ((frp->tname = strdup(tname)) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			goto err;
		}
		e_tname = 1;

		if (frp->fname == NULL) {
			F_SET(frp, FR_NONAME);
			frp->fname = frp->tname;
			frp->nlen = strlen(frp->fname);
		}
		oname = frp->tname;
		psize = 4 * 1024;

		F_SET(frp, FR_NEWFILE);
	} else {
		oname = frp->fname;

		/* Try to keep it at 10 pages or less per file. */
		if (sb.st_size < 40 * 1024)
			psize = 4 * 1024;
		else if (sb.st_size < 320 * 1024)
			psize = 32 * 1024;
		else
			psize = 64 * 1024;
	}
	
	/* Set up recovery. */
	memset(&oinfo, 0, sizeof(RECNOINFO));
	oinfo.bval = '\n';			/* Always set. */
	oinfo.psize = psize;
	oinfo.flags = F_ISSET(sp->gp, G_SNAPSHOT) ? R_SNAPSHOT : 0;
	if (rcv_fname == NULL) {
		if (rcv_tmp(sp, ep, frp->fname))
			msgq(sp, M_ERR,
		    "Modifications not recoverable if the system crashes.");
		else
			oinfo.bfname = ep->rcv_path;
	} else if ((ep->rcv_path = strdup(rcv_fname)) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		goto err;
	} else {
		e_rcv_path = 1;
		oinfo.bfname = ep->rcv_path;
		F_SET(ep, F_MODIFIED);
	}

	/* Open a db structure. */
	if ((ep->db = dbopen(rcv_fname == NULL ? oname : NULL,
	    O_NONBLOCK | O_RDONLY, DEFFILEMODE, DB_RECNO, &oinfo)) == NULL) {
		msgq(sp, M_ERR, "%s: %s", oname, strerror(errno));
		goto err;
	}

	/* Init file marks. */
	if (mark_init(sp, ep)) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		goto err;
	}

	/*
	 * The -R flag, or doing a "set readonly" during a session causes
	 * all files edited during the session (using an edit command, or
	 * even using tags) to be marked read-only.  Note that changing
	 * the file name (see ex/ex_file.c), clears this flag.
	 *
	 * Otherwise, try and figure out if a file is readonly.  This is
	 * a dangerous thing to do.  The kernel is the only arbiter of
	 * whether or not a file is writeable, and the best that a user
	 * program can do is guess.  Obvious loopholes are files that are
	 * on a file system mounted readonly (access catches this one some
	 * systems), or other protection mechanisms, ACL's for example,
	 * that we can't portably check.
	 *
	 * !!!
	 * Historic vi displayed the readonly message if none of the file
	 * write bits were set, or if an an access(2) call on the path
	 * failed.  This seems reasonable.  If the file is mode 444, root
	 * users may want to know that the owner of the file did not expect
	 * it to be written.
	 *
	 * Historic vi set the readonly bit if no write bits were set for
	 * a file, even if the access call would have succeeded.  This makes
	 * the superuser force the write even when vi expects that it will
	 * succeed.  I'm less supportive of this semantic, but it's historic
	 * practice and the conservative approach to vi'ing files as root.
	 *
	 * It would be nice if there was some way to update this when the user
	 * does a "^Z; chmod ...".  The problem is that we'd first have to
	 * distinguish between readonly bits set because of file permissions
	 * and those set for other reasons.  That's not too hard, but deciding
	 * when to reevaluate the permissions is trickier.  An alternative
	 * might be to turn off the readonly bit if the user forces a write
	 * and it succeeds.
	 *
	 * XXX
	 * Access(2) doesn't consider the effective uid/gid values.  This
	 * probably isn't a problem for vi when it's running standalone.
	 */
	F_CLR(frp, FR_RDONLY);
	if (O_ISSET(sp, O_READONLY) || !F_ISSET(frp, FR_NEWFILE) &&
	    (!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
	    access(frp->fname, W_OK)))
		F_SET(frp, FR_RDONLY);

	/* Flush the line caches. */
	ep->c_lno = ep->c_nlines = OOBLNO;

	/* Start logging. */
	log_init(sp, ep);

	++ep->refcnt;

	/* Close the previous file; if that fails, close the new one. */
	if (sp->ep != NULL && file_end(sp, sp->ep, force)) {
		(void)file_end(sp, ep, 1);
		return (1);
	}

	/*
	 * 4.4BSD supports locking in the open call, other systems don't.
	 * Since the user can't interrupt us between the open and here,
	 * it's a don't care.
	 *
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Assume that EAGAIN
	 * is the former.  There isn't a portable way to do this.
	 */
	if (flock(ep->db->fd(ep->db), LOCK_EX | LOCK_NB))
		if (errno == EAGAIN) {
			msgq(sp, M_INFO,
			    "%s already locked, session is read-only", oname);
			F_SET(frp, FR_RDONLY);
		} else
			msgq(sp, M_VINFO, "%s cannot be locked", oname);

	/* Switch... */
	sp->ep = ep;
	if ((sp->p_frp = sp->frp) != NULL)
		set_alt_fname(sp, sp->p_frp->fname);
	sp->frp = frp;
	return (0);

err:	if (e_rcv_path)
		FREE(ep->rcv_path, strlen(ep->rcv_path));
	if (e_tname)
		(void)unlink(frp->tname);
	if (e_ep)
		FREE(ep, sizeof(EXF));
	return (NULL);
}

/*
 * file_end --
 *	Stop editing a file.
 *
 *	NB: sp->ep MAY NOT BE THE SAME AS THE ARGUMENT ep, SO DON'T
 *	    USE IT!
 */
int
file_end(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	MARK *mp;
	int termsignal;

	/*
	 * Save the cursor location.
	 *
	 * XXX
	 * It would be cleaner to do this somewhere else, but by the time
	 * ex or vi know that we're changing files it's already happened.
	 */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* If multiply referenced, decrement count and return. */
	if (--ep->refcnt != 0)
		return (0);

	/*
	 * The HUP and TERM signal handlers use this routine.  If the
	 * S_TERMSIGNAL flag is set, we clean up and get out.  We very
	 * specifically don't muck with linked lists or messages.
	 * Check everything for a NULL value, this makes the "drop core"
	 * window quite small.
	 */
	termsignal = F_ISSET(sp, S_TERMSIGNAL);

	/* Close the db structure. */
	if (ep->db->close != NULL && ep->db->close(ep->db) && !force) {
		if (!termsignal)
		    msgq(sp, M_ERR,
		        "%s: close: %s", sp->frp->fname, strerror(errno));
		return (1);
	}

	/* Committed to the close.  There's no going back... */

	/* Delete the recovery file. */
	if (!F_ISSET(ep, F_RCV_NORM)) {
		if (ep->rcv_path != NULL)
			(void)unlink(ep->rcv_path);
		if (ep->rcv_mpath != NULL)
			(void)unlink(ep->rcv_mpath);
	}
	/* Free recovery memory. */
	if (!termsignal) {
		if (ep->rcv_path != NULL)
			FREE(ep->rcv_path, strlen(ep->rcv_path));
		if (ep->rcv_mpath != NULL)
			FREE(ep->rcv_mpath, strlen(ep->rcv_mpath));
	}

	/* Stop logging. */
	(void)log_end(sp, ep);

	/*
	 * Unlink any temporary file; if FR_NONAME set, don't free the
	 * memory referenced by tname, because it's also referenced by
	 * fname.  The screen end code will free it.
	 */
	if (sp->frp->tname != NULL) {
		if (unlink(sp->frp->tname) && !termsignal)
			msgq(sp, M_ERR,
			    "%s: remove: %s", sp->frp->tname, strerror(errno));
		if (!termsignal) {
			if (!F_ISSET(sp->frp, FR_NONAME))
				FREE(sp->frp->tname, strlen(sp->frp->tname));
			sp->frp->tname = NULL;
		}
	}

	/* Free up any marks. */
	while ((mp = ep->marks.next) != NULL && mp != (MARK *)&ep->marks) {
		HDR_DELETE(mp, next, prev, MARK);
		FREE(mp, sizeof(MARK));
	}

	if (!termsignal) {
		/* Delete the EXF structure from the chain. */
		HDR_DELETE(ep, next, prev, EXF);

		/* Free the EXF structure. */
		FREE(ep, sizeof(EXF));
	}
	return (0);
}

/*
 * file_write --
 *	Write the file to disk.  Historic vi had fairly convoluted
 *	semantics for whether or not writes would happen.  That's
 *	why all the flags.
 */
int
file_write(sp, ep, fm, tm, fname, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	char *fname;
	int flags;
{
	struct stat sb;
	FILE *fp;
	MARK from, to;
	u_long nlno, nch;
	int fd, oflags;
	char *msg;

	/*
	 * Don't permit writing to temporary files.  The problem is that
	 * if it's a temp file, and the user does ":wq", we write and quit,
	 * unlinking the temporary file.  Not what the user had in mind
	 * at all.  This test cannot be forced.
	 */
	if (fname == NULL && F_ISSET(sp->frp, FR_NONAME)) {
		msgq(sp, M_ERR, "No filename to which to write.");
		return (1);
	}

	/* Can't write files marked read-only, unless forced. */
	if (!LF_ISSET(FS_FORCE) &&
	    fname == NULL && F_ISSET(sp->frp, FR_RDONLY)) {
		if (LF_ISSET(FS_POSSIBLE))
			msgq(sp, M_ERR,
			    "Read-only file, not written; use ! to override.");
		else
			msgq(sp, M_ERR,
			    "Read-only file, not written.");
		return (1);
	}

	/* If not forced, not appending, and "writeany" not set ... */
	if (!LF_ISSET(FS_FORCE | FS_APPEND) && !O_ISSET(sp, O_WRITEANY)) {
		/* Don't overwrite anything but the original file. */
		if (fname != NULL && !stat(fname, &sb) ||
		    F_ISSET(sp->frp, FR_NAMECHANGED) &&
		    !stat(sp->frp->fname, &sb)) {
			if (fname == NULL)
				fname = sp->frp->fname;
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
		"%s exists, not written; use ! to override.", fname);
			else
				msgq(sp, M_ERR,
				    "%s exists, not written.", fname);
			return (1);
		}

		/*
		 * Don't write part of any existing file.  Only test for
		 * the original file, the previous test catches anything
		 * else.
		 */
		if (!LF_ISSET(FS_ALL) && fname == NULL &&
		    !F_ISSET(sp->frp, FR_NAMECHANGED) &&
		    !stat(sp->frp->fname, &sb)) {
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
				    "Use ! to write a partial file.");
			else
				msgq(sp, M_ERR, "Partial file, not written.");
			return (1);
		}
	}

	/*
	 * Figure out if the file already exists -- if it doesn't, we display
	 * the "new file" message.  The stat might not be necessary, but we
	 * just repeat it because it's easier than hacking the previous tests.
	 * The information is only used for the user message, so we can ignore
	 * the obvious race condition.  If overwriting a file other than the
	 * original file, and O_WRITEANY was what got us here (neither force
	 * nor append was set), display the "existing file" messsage.  Note,
	 * since we turn FR_NAMECHANGED off on a successful write, the latter
	 * message only appears once.  This is historic practice.
	 */
	if (stat(fname == NULL ? sp->frp->fname : fname, &sb))
		msg = ": new file";
	else if (!LF_ISSET(FS_FORCE | FS_APPEND) &&
	    (fname != NULL || F_ISSET(sp->frp, FR_NAMECHANGED)))
		msg = ": existing file";
	else
		msg = "";

	/* We no longer care where the name came from. */
	if (fname == NULL)
		fname = sp->frp->fname;

	/* Set flags to either append or truncate. */
	oflags = O_CREAT | O_WRONLY;
	if (LF_ISSET(FS_APPEND))
		oflags |= O_APPEND;
	else
		oflags |= O_TRUNC;

	/* Open the file. */
	if ((fd = open(fname, oflags, DEFFILEMODE)) < 0) {
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/*
	 * Once we've decided that we can actually write the file, it
	 * doesn't matter that the file name was changed -- if it was,
	 * we created the file.
	 */
	F_CLR(sp->frp, FR_NAMECHANGED);

	/* Use stdio for buffering. */
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/* Build fake addresses, if necessary. */
	if (fm == NULL) {
		from.lno = 1;
		from.cno = 0;
		fm = &from;
		if (file_lline(sp, ep, &to.lno))
			return (1);
		to.cno = 0;
		tm = &to;
	}

	/* Write the file. */
	if (ex_writefp(sp, ep, fname, fp, fm, tm, &nlno, &nch)) {
		if (!LF_ISSET(FS_APPEND))
			msgq(sp, M_ERR, "%s: WARNING: file truncated!", fname);
		return (1);
	}

	msgq(sp, M_INFO, "%s%s: %lu line%s, %lu characters.",
	    fname, msg, nlno, nlno == 1 ? "" : "s", nch);

	/* If wrote the entire file, clear the modified bit. */
	if (LF_ISSET(FS_ALL))
		F_CLR(ep, F_MODIFIED);

	return (0);
}
