/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2007, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                           Ken Stevens, Steve McClure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  ---
 *
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  linebuf.h: Simple line buffer
 * 
 *  Known contributors to this file:
 *     Markus Armbruster, 2007
 */

#ifndef LINEBUF_H
#define LINEBUF_H

#define LBUF_LEN_MAX 4096

struct lbuf {
    /* All members are private! */
    unsigned len;		/* strlen(line) */
    int full;			/* got a complete line, with newline? */
    char line[LBUF_LEN_MAX];	/* buffered line, zero-terminated */
};

extern void lbuf_init(struct lbuf *);
extern int lbuf_len(struct lbuf *);
extern int lbuf_full(struct lbuf *);
extern char *lbuf_line(struct lbuf *);
extern int lbuf_putc(struct lbuf *, char);

#endif