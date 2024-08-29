/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: vinfo.h,v 10.9 2024/08/28 10:26:19 skimo Exp $ (Berkeley) $Date: 2024/08/28 10:26:19 $
 */

typedef struct _vinfoh VINFOH;
LIST_HEAD(_vinfoh, _vinfo);
struct _vinfo {
	LIST_ENTRY(_vinfo) q;	/* Linked list of vinfo. */
	char *name;
	db_recno_t lno;
	size_t cno;
};

void vinfo_init(GS *gp);
void vinfo_update(GS *gp, char *name, db_recno_t lno, size_t cno);
int vinfo_get(GS *gp, char *name, db_recno_t *lno, size_t *cno);
void vinfo_save(GS *gp);
