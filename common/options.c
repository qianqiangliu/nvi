/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options.c,v 8.14 1993/10/04 10:14:24 bostic Exp $ (Berkeley) $Date: 1993/10/04 10:14:24 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

static int	 	 opts_abbcmp __P((const void *, const void *));
static int	 	 opts_cmp __P((const void *, const void *));
static OPTLIST const	*opts_prefix __P((char *));
static int	 	 opts_print __P((SCR *, OPTLIST const *, OPTION *));

static OPTLIST const optlist[] = {
/* O_ALTWERASE */
	{"altwerase",	NULL,		OPT_0BOOL,	0},
/* O_AUTOINDENT */
	{"autoindent",	NULL,		OPT_0BOOL,	0},
/* O_AUTOPRINT */
	{"autoprint",	NULL,		OPT_1BOOL,	0},
/* O_AUTOWRITE */
	{"autowrite",	NULL,		OPT_0BOOL,	0},
/* O_BEAUTIFY */
	{"beautify",	NULL,		OPT_0BOOL,	0},
/* O_CC */
	{"cc",		NULL,		OPT_STR,	0},
/* O_COLUMNS */
	{"columns",	f_columns,	OPT_NUM,	OPT_NOSAVE},
/* O_COMMENT */
	{"comment",	NULL,		OPT_0BOOL,	0},
/* O_DIGRAPH */
	{"digraph",	NULL,		OPT_0BOOL,	0},
/* O_DIRECTORY */
	{"directory",	NULL,		OPT_STR,	OPT_NOSAVE},
/* O_EDCOMPATIBLE */
	{"edcompatible",NULL,		OPT_0BOOL,	0},
/* O_ERRORBELLS */
	{"errorbells",	NULL,		OPT_0BOOL,	0},
/* O_EXRC */
	{"exrc",	NULL,		OPT_0BOOL,	0},
/* O_EXTENDED */
	{"extended",	NULL,		OPT_0BOOL,	0},
/* O_FLASH */
	{"flash",	NULL,		OPT_1BOOL,	0},
/* O_IGNORECASE */
	{"ignorecase",	NULL,		OPT_0BOOL,	0},
/* O_KEYTIME */
	{"keytime",	f_keytime,	OPT_NUM,	0},
/* O_LEFTRIGHT */
	{"leftright",	f_leftright,	OPT_0BOOL,	0},
/* O_LINES */
	{"lines",	f_lines,	OPT_NUM,	OPT_NOSAVE},
/* O_LISP */
	{"lisp",	f_lisp,		OPT_0BOOL,	0},
/* O_LIST */
	{"list",	f_list,		OPT_0BOOL,	0},
/* O_MAGIC */
	{"magic",	NULL,		OPT_1BOOL,	0},
/* O_MAKE */
	{"make",	NULL,		OPT_STR,	0},
/* O_MATCHTIME */
	{"matchtime",	f_matchtime,	OPT_NUM,	0},
/* O_MESG */
	{"mesg",	f_mesg,		OPT_1BOOL,	0},
/* O_MODELINE */
	{"modeline",	f_modelines,	OPT_0BOOL,	0},
/* O_MODELINES */
	{"modelines",	f_modelines,	OPT_0BOOL,	0},
/* O_NUMBER */
	{"number",	f_number,	OPT_0BOOL,	0},
/* O_NUNDO */
	{"nundo",	NULL,		OPT_0BOOL,	0},
/* O_OPEN */
	{"open",	NULL,		OPT_1BOOL,	0},
/* O_OPTIMIZE */
	{"optimize",	f_optimize,	OPT_1BOOL,	0},
/* O_PARAGRAPHS */
	{"paragraphs",	f_paragraph,	OPT_STR,	0},
/* O_PROMPT */
	{"prompt",	NULL,		OPT_1BOOL,	0},
/* O_READONLY */
	{"readonly",	f_readonly,	OPT_0BOOL,	0},
/* O_REDRAW */
	{"redraw",	NULL,		OPT_0BOOL,	0},
/* O_REMAP */
	{"remap",	NULL,		OPT_1BOOL,	0},
/* O_REPORT */
	{"report",	NULL,		OPT_NUM,	0},
/* O_RULER */
	{"ruler",	f_ruler,	OPT_0BOOL,	0},
/* O_SCROLL */
	{"scroll",	NULL,		OPT_NUM,	0},
/* O_SECTIONS */
	{"sections",	f_section,	OPT_STR,	0},
/* O_SHELL */
	{"shell",	NULL,		OPT_STR,	0},
/* O_SHIFTWIDTH */
	{"shiftwidth",	f_shiftwidth,	OPT_NUM,	0},
/* O_SHOWMATCH */
	{"showmatch",	NULL,		OPT_0BOOL,	0},
/* O_SHOWMODE */
	{"showmode",	NULL,		OPT_0BOOL,	0},
/* O_SIDESCROLL */
	{"sidescroll",	f_sidescroll,	OPT_NUM,	0},
/* O_TABSTOP */
	{"tabstop",	f_tabstop,	OPT_NUM,	0},
/* O_TAGLENGTH */
	{"taglength",	NULL,		OPT_NUM,	0},
/* O_TAGS */
	{"tags",	f_tags,		OPT_STR,	0},
/* O_TERM */
	{"term",	f_term,		OPT_STR,	OPT_NOSAVE},
/* O_TERSE */
	{"terse",	NULL,		OPT_0BOOL,	0},
/* O_TIMEOUT */
	{"timeout",	NULL,		OPT_0BOOL,	0},
/* O_VERBOSE */
	{"verbose",	NULL,		OPT_0BOOL,	0},
/* O_W300 */
	{"w300",	f_w300,		OPT_NUM,	OPT_NEVER},
/* O_W1200 */
	{"w1200",	f_w1200,	OPT_NUM,	OPT_NEVER},
/* O_W9600 */
	{"w9600",	f_w9600,	OPT_NUM,	OPT_NEVER},
/* O_WARN */
	{"warn",	NULL,		OPT_1BOOL,	0},
/* O_WINDOW */
	{"window",	f_window,	OPT_NUM,	0},
/* O_WRAPMARGIN */
	{"wrapmargin",	f_wrapmargin,	OPT_NUM,	0},
/* O_WRAPSCAN */
	{"wrapscan",	NULL,		OPT_1BOOL,	0},
/* O_WRITEANY */
	{"writeany",	NULL,		OPT_0BOOL,	0},
	{NULL},
};

typedef struct abbrev {
        char *name;
        int offset;
} OABBREV;

static OABBREV const abbrev[] = {
	{"ai",		O_AUTOINDENT},
	{"ap",		O_AUTOPRINT},
	{"aw",		O_AUTOWRITE},
	{"bf",		O_BEAUTIFY},
	{"cc",		O_CC},
	{"co",		O_COLUMNS},
	{"dig",		O_DIGRAPH},
	{"dir",		O_DIRECTORY},
	{"eb",		O_ERRORBELLS},
	{"ed",		O_EDCOMPATIBLE},
	{"fl",		O_FLASH},
	{"ic",		O_IGNORECASE},
	{"kt",		O_KEYTIME},
	{"li",		O_LIST},
	{"ls",		O_LINES},
	{"ma",		O_MAGIC},
	{"me",		O_MESG},
	{"mk",		O_MAKE},
	{"modeline",	O_MODELINES},
	{"nu",		O_NUMBER},
	{"opt",		O_OPTIMIZE},
	{"pa",		O_PARAGRAPHS},
	{"pr",		O_PROMPT},
	{"re",		O_REPORT},
	{"ro",		O_READONLY},
	{"ru",		O_RULER},
	{"sc",		O_SCROLL},
	{"se",		O_SECTIONS},
	{"sh",		O_SHELL},
	{"sm",		O_SHOWMATCH},
	{"ss",		O_SIDESCROLL},
	{"sw",		O_SHIFTWIDTH},
	{"te",		O_TERM},
	{"tr",		O_TERSE},
	{"ts",		O_TABSTOP},
	{"ve",		O_VERBOSE},
	{"wa",		O_WARN},
	{"wm",		O_WRAPMARGIN},
	{"wr",		O_WRITEANY},
	{"ws",		O_WRAPSCAN},
	{NULL},
};

/*
 * opts_init --
 *	Initialize some of the options.  Since the user isn't really
 *	"setting" these variables, don't set their OPT_SET bits.
 */
int
opts_init(sp)
	SCR *sp;
{
	OPTLIST const *op;
	speed_t speed;
	u_long sval;
	int cnt;
	char *s, *argv[2], b1[1024];

	argv[0] = b1;
	argv[1] = NULL;

#define	SET_DEF(opt, str) {						\
	if (str != b1)		/* GCC puts strings in text-space. */	\
		(void)strcpy(b1, str);					\
	if (opts_set(sp, argv))	{					\
		msgq(sp, M_ERR,						\
		    "Unable to set default %s option", optlist[opt]);	\
		return (1);						\
	}								\
	F_CLR(&sp->opts[opt], OPT_SET);					\
}
	/* Set default values. */
	for (op = optlist, cnt = 0; op->name != NULL; ++op, ++cnt)
		if (op->type == OPT_0BOOL)
			O_CLR(sp, cnt);
		else if (op->type == OPT_1BOOL)
			O_SET(sp, cnt);
			
	SET_DEF(O_CC, "cc=cc -c");
	(void)snprintf(b1, sizeof(b1), "directory=%s",
	    (s = getenv("TMPDIR")) == NULL ? _PATH_PRESERVE : s);
	SET_DEF(O_DIRECTORY, b1);
	SET_DEF(O_KEYTIME, "keytime=6");
	SET_DEF(O_MAKE, "make=make");
	SET_DEF(O_MATCHTIME, "matchtime=7");
	SET_DEF(O_REPORT, "report=5");
	SET_DEF(O_PARAGRAPHS, "paragraphs=IPLPPPQPP LIpplpipbp");
	(void)snprintf(b1, sizeof(b1), "scroll=%ld", O_VAL(sp, O_LINES) / 2);
	SET_DEF(O_SCROLL, b1);
	SET_DEF(O_SECTIONS, "sections=NHSHH HUnhsh");
	(void)snprintf(b1, sizeof(b1),
	    "shell=%s", (s = getenv("SHELL")) == NULL ? _PATH_BSHELL : s);
	SET_DEF(O_SHELL, b1);
	SET_DEF(O_SHIFTWIDTH, "shiftwidth=8");
	SET_DEF(O_SIDESCROLL, "sidescroll=16");
	SET_DEF(O_TABSTOP, "tabstop=8");
	(void)snprintf(b1, sizeof(b1), "tags=%s", _PATH_TAGS);
	SET_DEF(O_TAGS, b1);
	(void)snprintf(b1, sizeof(b1),
	    "term=%s", (s = getenv("TERM")) == NULL ? "unknown" : s);
	SET_DEF(O_TERM, b1);

	/*
	 * The default window option value is:
	 *		8 if baud rate <=  600
	 *	       16 if baud rate == 1200
	 *	LINES - 1 if baud rate  > 1200
	 */
	speed = cfgetospeed(&sp->gp->original_termios);
	if (speed <= 600)
		sval = 8;
	else if (speed <= 1200)
		sval = 16;
	else
		sval = O_VAL(sp, O_LINES) - 1;
	(void)snprintf(b1, sizeof(b1), "window=%lu", sval);
	SET_DEF(O_WINDOW, b1);
	SET_DEF(O_WRAPMARGIN, "wrapmargin=0");

	/*
	 * By default, the historic vi always displayed information
	 * about two options, redraw and term.  Term seems sufficient.
	 */
	F_SET(&sp->opts[O_TERM], OPT_SET);
	return (0);
}

/*
 * opts_set --
 *	Change the values of one or more options.
 */
int
opts_set(sp, argv)
	SCR *sp;
	char **argv;
{
	enum optdisp disp;
	OABBREV atmp, *ap;
	OPTLIST const *op;
	OPTLIST otmp;
	OPTION *spo;
	u_long value, turnoff;
	int ch, offset, rval;
	char *endp, *equals, *name, *p;
	
	disp = NO_DISPLAY;
	for (rval = 0; *argv; ++argv) {
		/*
		 * The historic vi dumped the options for each occurrence of
		 * "all" in the set list.  Puhleeze.
		 */
		if (!strcmp(*argv, "all")) {
			disp = ALL_DISPLAY;
			continue;
		}
			
		/* Find equals sign or end of set, skipping backquoted chars. */
		for (p = name = *argv, equals = NULL; ch = *p; ++p)
			switch(ch) {
			case '=':
				equals = p;
				break;
			case '\\':
				/* Historic vi just used the backslash. */
				if (p[1] == '\0')
					break;
				++p;
				break;
			}

		turnoff = 0;
		op = NULL;
		if (equals)
			*equals++ = '\0';

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
			op = optlist + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, optlist,
		    sizeof(optlist) / sizeof(OPTLIST) - 1,
		    sizeof(OPTLIST), opts_cmp)) != NULL)
			goto found;

		/* Try the name without any leading "no". */
		if (name[0] == 'n' && name[1] == 'o') {
			turnoff = 1;
			name += 2;
		} else
			goto prefix;

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
			op = optlist + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, optlist,
		    sizeof(optlist) / sizeof(OPTLIST) - 1,
		    sizeof(OPTLIST), opts_cmp)) != NULL)
			goto found;

		/* Check for prefix match. */
prefix:		op = opts_prefix(name);

found:		if (op == NULL) {
			msgq(sp, M_ERR,
			    "no %s option: 'set all' gives all option values",
			    name);
			continue;
		}

		/* Find current option values. */
		offset = op - optlist;
		spo = sp->opts + offset;

		/* Set name, value. */
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (equals) {
				msgq(sp, M_ERR,
				    "set: [no]%s option doesn't take a value",
				    name);
				break;
			}
			if (op->func != NULL) {
				if (op->func(sp, spo, NULL, turnoff)) {
					rval = 1;
					break;
				}
			} else if (turnoff)
				O_CLR(sp, offset);
			else
				O_SET(sp, offset);
			F_SET(&sp->opts[offset], OPT_SET);
			break;
		case OPT_NUM:
			if (turnoff) {
				msgq(sp, M_ERR,
				    "set: %s option isn't a boolean", name);
				break;
			}
			if (!equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}
			value = strtol(equals, &endp, 10);
			if (*endp && !isblank(*endp)) {
				msgq(sp, M_ERR,
				    "set %s: illegal number %s", name, equals);
				break;
			}
			if (op->func != NULL) {
				if (op->func(sp, spo, equals, value)) {
					rval = 1;
					break;
				}
			} else
				O_VAL(sp, offset) = value;
			F_SET(&sp->opts[offset], OPT_SET);
			break;
		case OPT_STR:
			if (turnoff) {
				msgq(sp, M_ERR,
				    "set: %s option isn't a boolean", name);
				break;
			}
			if (!equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}
			if (op->func != NULL) {
				if (op->func(sp, spo, equals, (u_long)0)) {
					rval = 1;
					break;
				}
			} else {
				if (F_ISSET(&sp->opts[offset], OPT_ALLOCATED))
					free(O_STR(sp, offset));
				if ((O_STR(sp, offset) =
				    strdup(equals)) == NULL) {
					msgq(sp, M_ERR,
					    "Error: %s", strerror(errno));
					rval = 1;
					break;
				} else
					F_SET(&sp->opts[offset], OPT_ALLOCATED);
			}
			F_SET(&sp->opts[offset], OPT_SET);
			break;
		default:
			abort();
		}
	}
	if (disp)
		opts_dump(sp, disp);
	return (rval);
}

/*
 * opt_dump --
 *	List the current values of selected options.
 */
void
opts_dump(sp, type)
	SCR *sp;
	enum optdisp type;
{
	OPTLIST const *op;
	int base, b_num, chcnt, cnt, col, colwidth, curlen, endcol, s_num;
	int numcols, numrows, row, tablen, termwidth;
	int b_op[O_OPTIONCOUNT], s_op[O_OPTIONCOUNT];
	char nbuf[20];

	/*
	 * Options are output in two groups -- those that fit at least two to
	 * a line and those that don't.  We do output on tab boundaries for no
	 * particular reason.   First get the set of options to list, keeping
	 * track of the length of each.  No error checking, because we know
	 * that O_TERM was set so at least one option has the OPT_SET bit on.
	 * Termwidth is the tab stop before half of the line in the first loop,
	 * and the full line length later on.
	 */
	colwidth = -1;
	tablen = O_VAL(sp, O_TABSTOP);
	termwidth = (sp->cols - 1) / 2 & ~(tablen - 1);
	for (b_num = s_num = 0, op = optlist; op->name; ++op) {
		cnt = op - optlist;

		/* If OPT_NEVER set, it's never displayed. */
		if (F_ISSET(op, OPT_NEVER))
			continue;

		switch (type) {
		case ALL_DISPLAY:		/* Display all. */
			break;
		case CHANGED_DISPLAY:		/* Display changed. */
			if (!F_ISSET(&sp->opts[cnt], OPT_SET))
				continue;
			break;
		case SELECT_DISPLAY:		/* Display selected. */
			if (!F_ISSET(&sp->opts[cnt], OPT_SELECTED))
				continue;
			break;
		default:
		case NO_DISPLAY:
			abort();
			/* NOTREACHED */
		}
		F_CLR(&sp->opts[cnt], OPT_SELECTED);

		curlen = strlen(op->name);
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (!O_ISSET(sp, cnt))
				curlen += 2;
			break;
		case OPT_NUM:
			(void)snprintf(nbuf,
			    sizeof(nbuf), "%ld", O_VAL(sp, cnt));
			curlen += strlen(nbuf);
			break;
		case OPT_STR:
			curlen += strlen(O_STR(sp, cnt)) + 3;
			break;
		}
		if (curlen < termwidth) {
			if (colwidth < curlen)
				colwidth = curlen;
			s_op[s_num++] = cnt;
		} else
			b_op[b_num++] = cnt;
	}

	colwidth = (colwidth + tablen) & ~(tablen - 1);
	termwidth = sp->cols - 1;
	numcols = termwidth / colwidth;
	if (s_num > numcols) {
		numrows = s_num / numcols;
		if (s_num % numcols)
			++numrows;
	} else
		numrows = 1;

	for (row = 0; row < numrows;) {
		endcol = colwidth;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += opts_print(sp,
			    &optlist[s_op[base]], &sp->opts[s_op[base]]);
			if ((base += numrows) >= s_num)
				break;
			while ((cnt =
			    (chcnt + tablen & ~(tablen - 1))) <= endcol) {
				(void)putc('\t', sp->stdfp);
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		if (++row < numrows || b_num)
			(void)putc('\n', sp->stdfp);
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(sp, &optlist[b_op[row]], &sp->opts[b_op[row]]);
		if (++row < b_num)
			(void)putc('\n', sp->stdfp);
	}
	(void)putc('\n', sp->stdfp);
}

/*
 * opts_save --
 *	Write the current configuration to a file.
 */
int
opts_save(sp, fp)
	SCR *sp;
	FILE *fp;
{
	OPTION *spo;
	OPTLIST const *op;
	int ch, cnt;
	char *p;

	for (spo = sp->opts, op = optlist; op->name; ++op) {
		if (F_ISSET(op, OPT_NOSAVE))
			continue;
		cnt = op - optlist;
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (O_ISSET(sp, cnt))
				(void)fprintf(fp, "set %s\n", op->name);
			else
				(void)fprintf(fp, "set no%s\n", op->name);
			break;
		case OPT_NUM:
			(void)fprintf(fp,
			    "set %s=%-3d\n", op->name, O_VAL(sp, cnt));
			break;
		case OPT_STR:
			for (p = op->name; (ch = *p) != '\0'; ++p) {
				if (isblank(ch))
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('=', fp);
			for (p = O_STR(sp, cnt); (ch = *p) != '\0'; ++p) {
				if (isblank(ch))
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('\n', fp);
			break;
		}
		if (ferror(fp)) {
			msgq(sp, M_ERR, "I/O error: %s", strerror(errno));
			return (1);
		}
	}
	return (0);
}

/*
 * opts_print --
 *	Print out an option.
 */
static int
opts_print(sp, op, spo)
	SCR *sp;
	OPTLIST const *op;
	OPTION *spo;
{
	int curlen, offset;

	curlen = 0;
	offset = op - optlist;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		if (!O_ISSET(sp, offset)) {
			curlen += 2;
			(void)putc('n', sp->stdfp);
			(void)putc('o', sp->stdfp);
		}
		curlen += fprintf(sp->stdfp, "%s", op->name);
		break;
	case OPT_NUM:
		curlen += fprintf(sp->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', sp->stdfp);
		curlen += fprintf(sp->stdfp, "%ld", O_VAL(sp, offset));
		break;
	case OPT_STR:
		curlen += fprintf(sp->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', sp->stdfp);
		curlen += 1;
		(void)putc('"', sp->stdfp);
		curlen += fprintf(sp->stdfp, "%s", O_STR(sp, offset));
		curlen += 1;
		(void)putc('"', sp->stdfp);
		break;
	}
	return (curlen);
}

/*
 * opts_prefix --
 *	Check to see if the name is the prefix of one (and only one)
 *	option.  If so, return the option.
 */
static OPTLIST const *
opts_prefix(name)
	char *name;
{
	OPTLIST const *op, *save_op;
	size_t len;

	save_op = NULL;
	len = strlen(name);
	for (op = optlist; op->name != NULL; ++op) {
		if (op->name[0] < name[0])
			continue;
		if (op->name[0] > name[0])
			break;
		if (!memcmp(op->name, name, len)) {
			if (save_op != NULL)
				return (NULL);
			save_op = op;
		}
	}
	return (save_op);
}

static int
opts_abbcmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OABBREV *)a)->name, ((OABBREV *)b)->name));
}

static int
opts_cmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OPTLIST *)a)->name, ((OPTLIST *)b)->name));
}
