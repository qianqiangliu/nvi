/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: vs_relative.c,v 10.9 1996/05/04 18:50:53 bostic Exp $ (Berkeley) $Date: 1996/05/04 18:50:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"

/*
 * vs_column --
 *	Return the logical column of the cursor in the line.
 *
 * PUBLIC: int vs_column __P((SCR *, size_t *));
 */
int
vs_column(sp, colp)
	SCR *sp;
	size_t *colp;
{
	VI_PRIVATE *vip;

	vip = VIP(sp);

	*colp = (O_ISSET(sp, O_LEFTRIGHT) ?
	    vip->sc_smap->coff : (vip->sc_smap->soff - 1) * sp->cols) +
	    vip->sc_col - (O_ISSET(sp, O_NUMBER) ? O_NUMBER_LENGTH : 0);
	return (0);
}

/*
 * vs_screens --
 *	Return the screens necessary to display the line, or if specified,
 *	the physical character column within the line, including space
 *	required for the O_NUMBER and O_LIST options.
 *
 * PUBLIC: size_t vs_screens __P((SCR *, recno_t, size_t *));
 */
size_t
vs_screens(sp, lno, cnop)
	SCR *sp;
	recno_t lno;
	size_t *cnop;
{
	size_t cols, screens;

#ifdef	DEBUG
	if (O_ISSET(sp, O_LEFTRIGHT))
		abort();
#endif

	/*
	 * Check for a cached value.  We maintain a cache because, if the
	 * line is large, this routine gets called repeatedly.  One other
	 * hack, lots of time the cursor is on column one, which is an easy
	 * one.
	 */
	if (cnop == NULL) {
		if (VIP(sp)->ss_lno == lno)
			return (VIP(sp)->ss_screens);
	} else if (*cnop == 0)
		return (1);

	/* Figure out how many columns the line/column needs. */
	cols = vs_columns(sp, NULL, lno, cnop, NULL);

	screens = (cols / sp->cols + (cols % sp->cols ? 1 : 0));
	if (screens == 0)
		screens = 1;

	/* Cache the value. */
	if (cnop == NULL) {
		VIP(sp)->ss_lno = lno;
		VIP(sp)->ss_screens = screens;
	}
	return (screens);
}

/*
 * vs_columns --
 *	Return the screen columns necessary to display the line, or,
 *	if specified, the physical character column within the line.
 *
 * PUBLIC: size_t vs_columns __P((SCR *, char *, recno_t, size_t *, size_t *));
 */
size_t
vs_columns(sp, lp, lno, cnop, diffp)
	SCR *sp;
	char *lp;
	recno_t lno;
	size_t *cnop, *diffp;
{
	size_t chlen, cno, last, len, scno, tab_off;
	int ch, listset;
	char *p;

	/* Need the line to go any further. */
	if (lp == NULL) {
		(void)db_get(sp, lno, 0, &lp, &len);
		if (len == 0)
			goto done;
	}

	/* Missing or empty lines are easy. */
	if (lp == NULL) {
done:		if (diffp != NULL)		/* XXX */
			*diffp = 0;
		return (0);
	}

	listset = O_ISSET(sp, O_LIST);

	p = lp;
	scno = tab_off = 0;

	/* Leading number if O_NUMBER option set. */
	if (O_ISSET(sp, O_NUMBER))
		scno += O_NUMBER_LENGTH;

#define	CHLEN(val) (ch = *(u_char *)p++) == '\t' &&			\
	    !listset ? TAB_OFF(val) : KEY_LEN(sp, ch);
#define	TAB_RESET {							\
	/*								\
	 * If past the end of the screen, and the character was a tab,	\
	 * reset the screen column to 0.  Otherwise, display the rest	\
	 * of the character on the next line.				\
	 */								\
	if ((tab_off += chlen) >= sp->cols)				\
		if (ch == '\t') {					\
			tab_off = 0;					\
			scno -= scno % sp->cols;			\
		} else							\
			tab_off -= sp->cols;				\
}
	if (cnop == NULL)
		while (len--) {
			chlen = CHLEN(tab_off);
			last = scno;
			scno += chlen;
			TAB_RESET;
		}
	else
		for (cno = *cnop;; --cno) {
			chlen = CHLEN(tab_off);
			last = scno;
			scno += chlen;
			TAB_RESET;
			if (cno == 0)
				break;
		}

	/* Trailing '$' if O_LIST option set. */
	if (O_ISSET(sp, O_LIST) && cnop == NULL)
		scno += KEY_LEN(sp, '$');

	/*
	 * The input screen code needs to know how much additional room
	 * tabs required.
	 */
	if (diffp != NULL)
		*diffp = scno - last;
	return (scno);
}

/*
 * vs_rcm --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position (which is stored as a screen column).
 *
 * PUBLIC: size_t vs_rcm __P((SCR *, recno_t, int));
 */
size_t
vs_rcm(sp, lno, islast)
	SCR *sp;
	recno_t lno;
	int islast;
{
	size_t len;

	/* Last character is easy, and common. */
	if (islast) {
		if (db_get(sp, lno, 0, NULL, &len) || len == 0)
			return (0);
		return (len - 1);
	}

	/* First character is easy, and common. */
	if (sp->rcm == 0)
		return (0);

	return (vs_colpos(sp, lno, sp->rcm));
}

/*
 * vs_colpos --
 *	Return the physical column from the line that will display a
 *	character closest to the specified screen column.
 *
 * PUBLIC: size_t vs_colpos __P((SCR *, recno_t, size_t));
 */
size_t
vs_colpos(sp, lno, cno)
	SCR *sp;
	recno_t lno;
	size_t cno;
{
	size_t chlen, len, llen, off, scno, tab_off;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	(void)db_get(sp, lno, 0, &lp, &llen);

	/* Missing or empty lines are easy. */
	if (lp == NULL || llen == 0)
		return (0);

	listset = O_ISSET(sp, O_LIST);

	/* Discard screen (logical) lines. */
	off = cno / sp->cols;
	cno %= sp->cols;
	for (scno = 0, p = lp, len = llen; off--;) {
		for (; len && scno < sp->cols; --len)
			scno += CHLEN(scno);

		/*
		 * If reached the end of the physical line, return
		 * the last physical character in the line.
		 */
		if (len == 0)
			return (llen - 1);

		/*
		 * If the character was a tab, reset the screen column to 0.
		 * Otherwise, the rest of the character is displayed on the
		 * next line.
		 */
		if (ch == '\t')
			scno = 0;
		else
			scno -= sp->cols;
	}

	/* Step through the line until reach the right character or EOL. */
	for (tab_off = scno; len--;) {
		chlen = CHLEN(tab_off);

		/*
		 * If we've reached the specific character, there are three
		 * cases.
		 *
		 * 1: scno == cno, i.e. the current character ends at the
		 *    screen character we care about.
		 *	a: off < llen - 1, i.e. not the last character in
		 *	   the line, return the offset of the next character.
		 *	b: else return the offset of the last character.
		 * 2: scno != cno, i.e. this character overruns the character
		 *    we care about, return the offset of this character.
		 */
		if ((scno += chlen) >= cno) {
			off = p - lp;
			return (scno == cno ?
			    (off < llen - 1 ? off : llen - 1) : off - 1);
		}

		TAB_RESET;
	}

	/* No such character; return the start of the last character. */
	return (llen - 1);
}
