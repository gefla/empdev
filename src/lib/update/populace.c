/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2006, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  populace.c: Return workforce available
 * 
 *  Known contributors to this file:
 *     Dave Pare, 1986
 */

#include <config.h>

#include "land.h"
#include "lost.h"
#include "path.h"
#include "update.h"

void
populace(struct natstr *np, struct sctstr *sp, int etu)
{
    double hap, pct;
    int n;
    int civ = sp->sct_item[I_CIVIL];
    int mil = sp->sct_item[I_MILIT];

    if (civ == 0 && mil > 0) {
	sp->sct_work = 100;
	sp->sct_loyal = 0;
	sp->sct_oldown = sp->sct_own;
    }
    if (!civ && !mil && !sp->sct_item[I_UW]
	&& !has_units(sp->sct_x, sp->sct_y, sp->sct_own, 0)) {
	makelost(EF_SECTOR, sp->sct_own, 0, sp->sct_x, sp->sct_y);
	sp->sct_own = 0;
	sp->sct_oldown = 0;
	return;
    }
    if (sp->sct_own != sp->sct_oldown && sp->sct_loyal == 0) {
	sp->sct_oldown = sp->sct_own;
    }

    hap = np->nat_level[NAT_HLEV];
    pct = hap_req(np);
    if (sp->sct_own == sp->sct_oldown && hap < pct &&
	chance((pct - hap) / 5.0)) {
	/*
	 * zap the loyalty of unhappy civilians.
	 * there is a 20% chance per hap point below the
	 * "recommended" amount of this happening.
	 */
	n = roundavg(etu * 0.125);
	if (n == 0)
	    n = 1;
	n = sp->sct_loyal + (random() % n) + 1;
	if (n > 127)
	    n = 127;
	sp->sct_loyal = n;
    }
    if (sp->sct_loyal > 65 && mil < civ / 20) {
	int work_red;

	work_red = sp->sct_loyal - (50 + (random() % 15));
	n = sp->sct_work - work_red;
	if (n < 0)
	    n = 0;
	sp->sct_work = n;
	if (chance(work_red / 1000.0)) {
	    /*
	     * small chance of rebellion...
	     * if work_red is (max) 67,
	     * then revolt chance is 6.7%
	     */
	    revolt(sp);
	} else if (chance(.30) && sp->sct_own)
	    wu(0, sp->sct_own, "Civil unrest in %s!\n", ownxy(sp));
    }
    if (sp->sct_loyal) {
	n = sp->sct_loyal;
	if (chance(0.75))
	    n -= roundavg(etu * 0.25);
	else
	    n += roundavg(etu * 0.125);
	if (n < 0)
	    n = 0;
	else if (n > 127)
	    n = 127;
	sp->sct_loyal = n;
	if (sp->sct_loyal == 0) {
	    if (sp->sct_oldown != sp->sct_own) {
		wu(0, sp->sct_own,
		   "Sector %s is now fully yours\n", ownxy(sp));
		sp->sct_oldown = sp->sct_own;
	    }
	}
    }
    return;
}

int
total_work(int sctwork, int etu, int civil, int milit, int uw, int maxpop)
{
    if (civil > maxpop)
	civil = maxpop;
    if (milit > maxpop)
	milit = maxpop;
    if (uw > maxpop)
	uw = maxpop;

    return (civil * sctwork / 100.0 + milit * 0.4 + uw) * etu / 100.0;
}
