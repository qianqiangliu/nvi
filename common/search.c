/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 8.14 1993/09/13 19:33:41 bostic Exp $ (Berkeley) $Date: 1993/09/13 19:33:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"

static int	check_delta __P((SCR *, EXF *, long, recno_t));
static int	check_word __P((SCR *, char **, int *));
static int	ctag_conv __P((SCR *, char **, int *));
static int	get_delta __P((SCR *, char **, long *, u_int *));
static int	resetup __P((SCR *, regex_t **, enum direction,
		    char *, char **, long *, u_int *));
static void	search_intr __P((int));

/*
 * resetup --
 *	Set up a search for a regular expression.
 */
static int
resetup(sp, rep, dir, ptrn, epp, deltap, flagp)
	SCR *sp;
	regex_t **rep;
	enum direction dir;
	char *ptrn, **epp;
	long *deltap;
	u_int *flagp;
{
	u_int flags;
	int delim, eval, re_flags, replaced;
	char *p, *t;

	/* Set return information the default. */
	*deltap = 0;

	/*
	 * Use saved pattern if no pattern supplied, or if only a delimiter
	 * character is supplied.  Only the pattern was saved, historic vi
	 * did not reuse any delta supplied.
	 */
	if (ptrn == NULL || ptrn[1] == '\0') {
		if (!F_ISSET(sp, S_RE_SET)) {
noprev:			msgq(sp, M_INFO, "No previous search pattern.");
			return (1);
		}
		*rep = &sp->sre;
		return (0);
	}

	re_flags = 0;				/* Set RE flags. */
	if (O_ISSET(sp, O_EXTENDED))
		re_flags |= REG_EXTENDED;
	if (O_ISSET(sp, O_IGNORECASE))
		re_flags |= REG_ICASE;

	flags = *flagp;
	if (LF_ISSET(SEARCH_PARSE)) {		/* Parse the string. */
		/* Set delimiter. */
		delim = *ptrn++;

		/* Find terminating delimiter, handling escaped delimiters. */
		for (p = t = ptrn;;) {
			if (p[0] == '\0' || p[0] == delim) {
				if (p[0] == delim)
					++p;
				*t = '\0';
				break;
			}
			if (p[1] == delim && p[0] == '\\')
				++p;
			*t++ = *p++;
		}

		/*
		 * If characters after the terminating delimiter, it may
		 * be an error, or may be an offset.  In either case, we
		 * return the end of the string, whatever it may be, or
		 * change the end pointer to reference a NULL.  Don't just
		 * whack the string, in case it's text space.
		 */
		if (*p) {
			if (get_delta(sp, &p, deltap, flagp))
				return (1);
			if (LF_ISSET(SEARCH_TERM)) {
				msgq(sp, M_ERR,
			"Characters after search string and/or delta.");
				return (1);
			}
			if (epp != NULL)
				*epp = p;
		} else {
			/*
			 * STATIC: NEVER WRITTEN.
			 * Can't be const, because the normal case isn't.
			 */
			static char ebuf[1];
			if (epp != NULL)
				*epp = ebuf;
		}

		/* If the pattern was empty, use the previous pattern. */
		if (ptrn == NULL || *ptrn == '\0') {
			if (!F_ISSET(sp, S_RE_SET))
				goto noprev;
			*rep = &sp->sre;
			return (0);
		}

		/* Replace any word search pattern. */
		if (check_word(sp, &ptrn, &replaced))
			return (1);
	} else if (LF_ISSET(SEARCH_TAG)) {
		if (ctag_conv(sp, &ptrn, &replaced))
			return (1);
		re_flags &= ~REG_EXTENDED;
	}

	/* Compile the RE. */
	if (eval = regcomp(*rep, ptrn, re_flags))
		re_error(sp, eval, *rep);
	else if (LF_ISSET(SEARCH_SET)) {
		F_SET(sp, S_RE_SET);
		sp->searchdir = dir;
		sp->sre = **rep;
	}

	/* Free up any extra memory. */
	if (replaced)
		FREE_SPACE(sp, ptrn, 0);
	return (eval);
}

/*
 * Search interrupts.
 *
 * ISIG turns on VINTR, VQUIT and VSUSP.  We want VINTR to interrupt the
 * search, so we install a handler.  VQUIT is ignored by main() because
 * nvi never wants to catch it.  A handler for VSUSP should have been
 * installed by the screen code.
 */
#define	SET_UP_INTERRUPTS {						\
	if ((saveintr =							\
	    signal(SIGINT, search_intr)) != (sig_ret_t)-1) {		\
		F_CLR(sp, S_INTERRUPTED);				\
		F_SET(sp, S_INTERRUPTIBLE);				\
		if (tcgetattr(STDIN_FILENO, &term)) {			\
			msgq(sp, M_ERR,					\
			    "tcgetattr: %s", strerror(errno));		\
			return (1);					\
		}							\
		nterm = term;						\
		nterm.c_lflag |= ISIG;					\
		if (tcsetattr(STDIN_FILENO,				\
		    TCSANOW | TCSASOFT, &nterm)) {			\
			msgq(sp, M_ERR,					\
			    "tcsetattr: %s", strerror(errno));		\
			return (1);					\
		}							\
	}								\
}

#define	TEAR_DOWN_INTERRUPTS {						\
	if (saveintr != (sig_ret_t)-1) {				\
		if (signal(SIGINT, saveintr) == (sig_ret_t)-1)		\
			msgq(sp, M_ERR,					\
			    "signal: %s", strerror(errno));		\
		if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &term))	\
			msgq(sp, M_ERR,					\
			    "tcsetattr: %s", strerror(errno));		\
	}								\
}


#define	EMPTYMSG	"File empty; nothing to search."
#define	EOFMSG		"Reached end-of-file without finding the pattern."
#define	NOTFOUND	"Pattern not found."
#define	SOFMSG		"Reached top-of-file without finding the pattern."
#define	WRAPMSG		"Search wrapped."

int
f_search(sp, ep, fm, rm, ptrn, eptrn, flagp)
	SCR *sp;
	EXF *ep;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int *flagp;
{
	struct termios nterm, term;
	regmatch_t match[1];
	regex_t *re, lre;
	recno_t lno;
	sig_ret_t saveintr;
	size_t coff, len;
	long delta;
	u_int flags;
	int eval, rval, wrapped;
	char *l;

	if (file_lline(sp, ep, &lno))
		return (1);
	flags = *flagp;
	if (lno == 0) {
		if (LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, FORWARD, ptrn, eptrn, &delta, flagp))
		return (1);

	/*
	 * Start searching immediately after the cursor.  If at the end of the
	 * line, start searching on the next line.  This is incompatible (read
	 * bug fix) with the historic vi -- searches for the '$' pattern never
	 * moved forward, and "-t foo" didn't work if "foo" was the first thing
	 * in the file.
	 */
	if (LF_ISSET(SEARCH_FILE)) {
		lno = 1;
		coff = 0;
	} else {
		if ((l = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		if (fm->cno + 1 >= len) {
			if (fm->lno == lno) {
				if (!O_ISSET(sp, O_WRAPSCAN)) {
					if (LF_ISSET(SEARCH_MSG))
						msgq(sp, M_INFO, EOFMSG);
					return (1);
				}
				lno = 1;
			} else
				lno = fm->lno + 1;
			coff = 0;
		} else {
			lno = fm->lno;
			coff = fm->cno + 1;
		}
	}

	/*
	 * Set up busy message, interrupts.
	 *
	 * F_search is called from the ex_tagfirst() routine, which runs
	 * before the screen really exists.  Make sure we don't step on
	 * anything.
	 */
	if (sp->s_position != NULL)
		busy_on(sp, 1, "Searching...");

	SET_UP_INTERRUPTS;

	for (rval = 1, wrapped = 0;; ++lno, coff = 0) {
		if (F_ISSET(sp, S_INTERRUPTED)) {
			msgq(sp, M_INFO, "Interrupted.");
			break;
		}
		if (wrapped && lno > fm->lno ||
		    (l = file_gline(sp, ep, lno, &len)) == NULL) {
			if (wrapped) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, EOFMSG);
				break;
			}
			lno = 0;
			wrapped = 1;
			continue;
		}

		/* If already at EOL, just keep going. */
		if (len && coff == len)
			continue;

		/* Set the termination. */
		match[0].rm_so = coff;
		match[0].rm_eo = len;

#if DEBUG && 0
		TRACE(sp, "F search: %lu from %u to %u\n",
		    lno, coff, len ? len - 1 : len);
#endif
		/* Search the line. */
		eval = regexec(re, l, 1, match,
		    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, re);
			break;
		}
		
		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, WRAPMSG);

		/*
		 * If an offset, see if it's legal.  It's possible to match
		 * past the end of the line with $, so check for that case.
		 */
		if (delta) {
			if (check_delta(sp, ep, delta, lno))
				break;
			rm->lno = delta + lno;
			rm->cno = 0;
		} else {
#if DEBUG && 0
			TRACE(sp, "found: %qu to %qu\n",
			    match[0].rm_so, match[0].rm_eo);
#endif
			rm->lno = lno;
			rm->cno = match[0].rm_so;

			/*
			 * If a change command, it's possible to move beyond
			 * the end of a line.  Historic vi generally got this
			 * wrong (try "c?$<cr>").  Not all that sure this gets
			 * it right, there are lots of strange cases.
			 */
			if (!LF_ISSET(SEARCH_EOL) && rm->cno >= len)
				rm->cno = len ? len - 1 : 0;
		}
		rval = 0;
		break;
	}

	/* Turn off busy message, interrupts. */
	if (sp->s_position != NULL)
		busy_off(sp);

	TEAR_DOWN_INTERRUPTS;

	return (rval);
}

int
b_search(sp, ep, fm, rm, ptrn, eptrn, flagp)
	SCR *sp;
	EXF *ep;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int *flagp;
{
	struct termios nterm, term;
	regmatch_t match[1];
	regex_t *re, lre;
	recno_t lno;
	sig_ret_t saveintr;
	size_t coff, len, last;
	long delta;
	u_int flags;
	int eval, rval, wrapped;
	char *l;

	if (file_lline(sp, ep, &lno))
		return (1);
	flags = *flagp;
	if (lno == 0) {
		if (LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, BACKWARD, ptrn, eptrn, &delta, flagp))
		return (1);

	/* If in the first column, start searching on the previous line. */
	if (fm->cno == 0) {
		if (fm->lno == 1) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, SOFMSG);
				return (1);
			}
		} else
			lno = fm->lno - 1;
	} else
		lno = fm->lno;

	/* Turn on busy message, interrupts. */
	busy_on(sp, 1, "Searching...");

	SET_UP_INTERRUPTS;

	for (rval = 1, wrapped = 0, coff = fm->cno;; --lno, coff = 0) {
		if (F_ISSET(sp, S_INTERRUPTED)) {
			msgq(sp, M_INFO, "Interrupted.");
			break;
		}
		if (wrapped && lno < fm->lno || lno == 0) {
			if (wrapped) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, SOFMSG);
				break;
			}
			if (file_lline(sp, ep, &lno))
				goto err;
			if (lno == 0) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, EMPTYMSG);
				break;
			}
			++lno;
			wrapped = 1;
			continue;
		}

		if ((l = file_gline(sp, ep, lno, &len)) == NULL)
			goto err;

		/* Set the termination. */
		match[0].rm_so = 0;
		match[0].rm_eo = coff ? coff : len;

#if DEBUG && 0
		TRACE(sp, "B search: %lu from 0 to %qu\n", lno, match[0].rm_eo);
#endif
		/* Search the line. */
		eval = regexec(re, l, 1, match,
		    (match[0].rm_eo == len ? 0 : REG_NOTEOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, re);
			break;
		}

		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, WRAPMSG);
		
		if (delta) {
			if (check_delta(sp, ep, delta, lno))
				break;
			rm->lno = delta + lno;
			rm->cno = 0;
		} else {
#if DEBUG && 0
			TRACE(sp, "found: %qu to %qu\n",
			    match[0].rm_so, match[0].rm_eo);
#endif
			/*
			 * Find the last acceptable one in this line.  This
			 * is really painful, we need a cleaner interface to
			 * regexec to make this possible.
			 */
			for (;;) {
				last = match[0].rm_so;
				match[0].rm_so = match[0].rm_eo + 1;
				if (match[0].rm_so >= len ||
				    coff && match[0].rm_so >= coff)
					break;
				match[0].rm_eo = coff ? coff : len;
				eval = regexec(re, l, 1, match,
				    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) |
				    REG_STARTEND);
				if (eval == REG_NOMATCH)
					break;
				if (eval != 0) {
					re_error(sp, eval, re);
					goto err;
				}
			}
			rm->lno = lno;

			/* See comment in f_search(). */
			if (!LF_ISSET(SEARCH_EOL) && last >= len)
				rm->cno = len ? len - 1 : 0;
			else
				rm->cno = last;
		}
		rval = 0;
		break;
	}

	/* Turn off busy message, interrupts. */
err:	busy_off(sp);

	TEAR_DOWN_INTERRUPTS;

	return (rval);
}

/*
 * check_word --
 *	Vi special cases the pattern "\<ptrn\>", doing "word" searches.
 */
static int
check_word(sp, ptrnp, replacedp)
	SCR *sp;
	char **ptrnp;
	int *replacedp;
{
	size_t blen, needspace;
	int cnt;
	char *bp, *p, *t;

	/* Count up the "word" patterns. */
	*replacedp = 0;
	for (p = *ptrnp, cnt = 0; *p; ++p)
		if (p[0] == '\\' && p[1] && (p[1] == '<' || p[1] == '>'))
			++cnt;
	if (cnt == 0)
		return (0);

	/* Get enough memory to hold the final pattern. */
	needspace =
	    strlen(*ptrnp) + cnt * (sizeof(RE_WSTART) + sizeof(RE_WSTOP));
	GET_SPACE(sp, bp, blen, needspace);

	for (p = *ptrnp, t = bp; *p;)
		if (p[0] == '\\' && p[1] && (p[1] == '<' || p[1] == '>')) {
			if (p[1] == '<') {
				memmove(t, RE_WSTART, sizeof(RE_WSTART) - 1);
				t += sizeof(RE_WSTART) - 1;
			} else {
				memmove(t, RE_WSTOP, sizeof(RE_WSTOP) - 1);
				t += sizeof(RE_WSTOP) - 1;
			}
			p += 2;
		} else
			*t++ = *p++;
	*t = '\0';

	*ptrnp = bp;
	*replacedp = 1;
	return (0);
}

/*
 * ctag_conv --
 *	Convert a tags search path into something that regex can handle.
 */
static int
ctag_conv(sp, ptrnp, replacedp)
	SCR *sp;
	char **ptrnp;
	int *replacedp;
{
	size_t blen, len;
	int lastdollar;
	char *bp, *p, *t;

	*replacedp = 0;

	len = strlen(p = *ptrnp);

	/* Max memory usage is 2 times the length of the string. */
	GET_SPACE(sp, bp, blen, len * 2);

	t = bp;

	/* The last charcter is a '/' or '?', we just strip it. */
	if (p[len - 1] == '/' || p[len - 1] == '?')
		p[len - 1] = '\0';

	/* The next-to-last character is a '$', and it's magic. */
	if (p[len - 2] == '$') {
		lastdollar = 1;
		p[len - 2] = '\0';
	} else
		lastdollar = 0;

	/* The first character is a '/' or '?', we just strip it. */
	if (p[0] == '/' || p[0] == '?')
		++p;

	/* The second character is a '^', and it's magic. */
	if (p[0] == '^')
		*t++ = *p++;
		
	/*
	 * Escape every other magic character we can find, stripping the
	 * backslashes ctags inserts to escape the search delimiter
	 * characters.
	 */
	while (p[0]) {
		/* Ctags escapes the search delimiter characters. */
		if (p[0] == '\\' && (p[1] == '/' || p[1] == '?'))
			++p;
		else if (strchr("^.[$*", p[0]))
			*t++ = '\\';
		*t++ = *p++;
	}
	if (lastdollar)
		*t++ = '$';
	*t++ = '\0';

	*ptrnp = bp;
	*replacedp = 1;
	return (0);
}

/*
 * get_delta --
 *	Get a line delta.  The trickiness is that the delta can be pretty
 *	complicated, i.e. "+3-2+3++-" is allowed.
 *
 * !!!
 * In historic vi, if you had a delta on a search pattern which was used as
 * a motion command, the command became a line mode command regardless of the
 * cursor positions.  A fairly common trick is to use a delta of "+0" to make
 * the command a line mode command.  This is the only place that knows about
 * delta's, so we set the return flag information here.
 */
static int
get_delta(sp, dp, valp, flagp)
	SCR *sp;
	char **dp;
	long *valp;
	u_int *flagp;
{
	long val, tval;

	for (tval = 0; **dp != '\0'; *flagp |= SEARCH_DELTA) {
		if (**dp == '+' || **dp == '-') {
			if (!isdigit(*(*dp + 1))) {
				if (**dp == '+') {
					if (tval == LONG_MAX)
						goto overflow;
					++tval;
				} else {
					if (tval == LONG_MIN)
						goto underflow;
					--tval;
				}
				++dp;
				continue;
			}
		} else
			if (!isdigit(**dp))
				break;

		errno = 0;
		val = strtol(*dp, dp, 10);
		if (errno == ERANGE) {
			if (val == LONG_MAX)
overflow:			msgq(sp, M_ERR, "Delta value overflow.");
			else if (val == LONG_MIN)
underflow:			msgq(sp, M_ERR, "Delta value underflow.");
			else
				msgq(sp, M_ERR, "Error: %s.", strerror(errno));
			return (1);
		}
		if (val >= 0)
			if (LONG_MAX - val < tval)
				goto overflow;
		else
			if (-(LONG_MIN - tval) > val)
				goto underflow;
		tval += val;
	}
	*valp = tval;
	return (0);
}

/*
 * check_delta --
 *	Check a line delta to see if it's legal.
 */
static int
check_delta(sp, ep, delta, lno)
	SCR *sp;
	EXF *ep;
	long delta;
	recno_t lno;
{
	/* A delta can overflow a record number. */
	if (delta < 0) {
		if (lno < LONG_MAX && delta >= (long)lno) {
			msgq(sp, M_ERR, "Search offset before line 1.");
			return (1);
		}
	} else {
		if (ULONG_MAX - lno < delta) {
			msgq(sp, M_ERR, "Delta value overflow.");
			return (1);
		}
		if (file_gline(sp, ep, lno + delta, NULL) == NULL) {
			msgq(sp, M_ERR, "Search offset past end-of-file.");
			return (1);
		}
	}
	return (0);
}

/*
 * re_error --
 *	Report a regular expression error.
 */
void
re_error(sp, errcode, preg)
	SCR *sp;
	int errcode;
	regex_t *preg;
{
	size_t s;
	char *oe;

	s = regerror(errcode, preg, "", 0);
	if ((oe = malloc(s)) == NULL)
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
	else {
		(void)regerror(errcode, preg, oe, s);
		msgq(sp, M_ERR, "RE error: %s", oe);
	}
	free(oe);
}

/*
 * search_intr --
 *	Set the interrupt bit in any screen that is interruptible.
 *
 * XXX
 * In the future this may be a problem.  The user should be able to move to
 * another screen and keep typing while this runs.  If so, and the user has
 * more than one search/global (see ex/ex_global.c) running, it will be hard
 * to decide which one to stop.
 */
static void
search_intr(signo)
	int signo;
{
	SCR *sp;

	for (sp = __global_list->scrhdr.next;
	     sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
		if (F_ISSET(sp, S_INTERRUPTIBLE))
			F_SET(sp, S_INTERRUPTED);
}
