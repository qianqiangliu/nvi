/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define VINFO_FILE	".vinfo"

#ifndef lint
static const char sccsid[] = "$Id: vinfo.c,v 10.9 2024/08/28 10:26:19 skimo Exp $ (Berkeley) $Date: 2024/08/28 10:26:19 $";
#endif /* not lint */

static VINFO	*vinfo_search __P((GS *gp, char *name));
static void	 vinfo_add __P((GS *gp, char *name, db_recno_t lno, size_t cno));
static void	 vinfo_del __P((GS *gp, char *name));

void
vinfo_init(GS *gp)
{
	char *dir;
	char path[PATH_MAX];
	FILE *fp;
	char tmp[PATH_MAX+32];

	if ((dir = getenv("HOME")) == NULL)
		return;

	snprintf(path, sizeof(path), "%s/%s", dir, VINFO_FILE);

	if ((fp = fopen(path, "r")) == NULL)
		return;

	while (fgets(tmp, sizeof(tmp), fp)) {
		int lno, cno;
		if (sscanf(tmp, "%s\t%d\t%d\n", path, &lno, &cno) == 3) {
			vinfo_add(gp, path, lno, cno);
		}
	}

	fclose(fp);
}

static void
vinfo_add(GS *gp, char *name, db_recno_t lno, size_t cno)
{
	VINFO *vi_c, *vi_n;
	int len = strlen(name);

	CALLOC_GOTO(sp, vi_n, VINFO *, 1, sizeof(VINFO));
	CALLOC_GOTO(sp, vi_n->name, char *, 1, len + 1);

	memmove(vi_n->name, name, len);
	vi_n->lno = lno;
	vi_n->cno = cno;

	if ((vi_c = gp->vinfoq.lh_first) == NULL) {
		LIST_INSERT_HEAD(&gp->vinfoq, vi_n, q);
	} else {
		for (; vi_c->q.le_next != NULL; vi_c = vi_c->q.le_next);
		LIST_INSERT_AFTER(vi_c, vi_n, q);
	}

	return;

alloc_err:
	if (vi_n != NULL)
		free(vi_n);
}

static void
vinfo_del(GS *gp, char *name)
{
	VINFO *vi_c;

	if ((vi_c = vinfo_search(gp, name))) {
		LIST_REMOVE(vi_c, q);
		free(vi_c->name);
		free(vi_c);
	}
}

void
vinfo_update(GS *gp, char *name, db_recno_t lno, size_t cno)
{
	VINFO *vi_c;

	if ((vi_c = vinfo_search(gp, name))) {
		vi_c->lno = lno;
		vi_c->cno = cno;
	} else {
		vinfo_add(gp, name, lno, cno);
	}
}

static VINFO *
vinfo_search(GS *gp, char *name)
{
	VINFO *vi_c;

	for (vi_c = gp->vinfoq.lh_first; vi_c != NULL; vi_c = vi_c->q.le_next) {
		if (strcmp(vi_c->name, name) == 0) {
			return vi_c;
		}
	}

	return NULL;
}

int
vinfo_get(GS *gp, char *name, db_recno_t *lno, size_t *cno)
{
	VINFO *vi_c;

	if ((vi_c = vinfo_search(gp, name))) {
		*lno = vi_c->lno;
		*cno = vi_c->cno;
		return 0;
	}

	return -1;
}

void
vinfo_save(GS *gp)
{
	char *dir;
	char path[PATH_MAX];
	FILE *fp;
	char tmp[PATH_MAX+32];
	VINFO *vi_c;

	if ((dir = getenv("HOME")) == NULL)
		return;

	snprintf(path, sizeof(path), "%s/%s", dir, VINFO_FILE);

	if ((fp = fopen(path, "w")) == NULL)
		return;

	for (vi_c = gp->vinfoq.lh_first; vi_c != NULL; vi_c = vi_c->q.le_next) {
		snprintf(tmp, sizeof(tmp), "%s\t%d\t%ld\n", vi_c->name, vi_c->lno, (long)vi_c->cno);
		fputs(tmp, fp);
	}

	fclose(fp);
}
