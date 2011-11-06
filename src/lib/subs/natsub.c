/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2011, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                Ken Stevens, Steve McClure, Markus Armbruster
 *
 *  Empire is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  nat.c: Nation subroutines
 *
 *  Known contributors to this file:
 *     Markus Armbruster, 2009-2011
 *     Ron Koenderink, 2008-2009
 */

#include <config.h>

#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "file.h"
#include "nat.h"
#include "optlist.h"
#include "prototypes.h"
#include "tel.h"

/*
 * Initialize NATP for country #CNUM in status STAT.
 * STAT must be STAT_UNUSED, STAT_NEW, STAT_VIS or STAT_GOD.
 * Also wipe realms and telegrams.
 */
struct natstr *
nat_reset(struct natstr *natp, natid cnum, enum nat_status stat)
{
    struct realmstr newrealm;
    char buf[1024];
    int i;

    ef_blank(EF_NATION, cnum, natp);
    natp->nat_stat = stat;
    for (i = 0; i < MAXNOR; i++) {
	ef_blank(EF_REALM, i + cnum * MAXNOR, &newrealm);
	putrealm(&newrealm);
    }
    close(creat(mailbox(buf, cnum),
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
    /* FIXME natp->nat_ann = #annos */
    natp->nat_level[NAT_HLEV] = start_happiness;
    natp->nat_level[NAT_RLEV] = start_research;
    natp->nat_level[NAT_TLEV] = start_technology;
    natp->nat_level[NAT_ELEV] = start_education;
    for (i = 0; i < MAXNOC; i++)
	natp->nat_relate[i] = NEUTRAL;
    natp->nat_flags =
	NF_FLASH | NF_BEEP | NF_COASTWATCH | NF_SONAR | NF_TECHLISTS;
    return natp;
}

int
check_nat_name(char *cname, natid cnum)
{
    struct natstr *natp;
    natid cn;
    int allblank;
    char *p;

    if (strlen(cname) >= sizeof(natp->nat_cnam)) {
	pr("Country name too long\n");
	return 0;
    }

    allblank = 1;
    for (p = cname; *p != '\0'; p++) {
	if (iscntrl(*p)) {
	    pr("No control characters allowed in country names!\n");
	    return 0;
	} else if (!isspace(*p))
	    allblank = 0;
    }
    if (allblank) {
	pr("Country name can't be all blank\n");
	return 0;
    }

    for (cn = 0; NULL != (natp = getnatp(cn)); cn++) {
	if (cn != cnum && !strcmp(cname, natp->nat_cnam)) {
	    pr("Country #%d is already called `%s'\n", cn, cname);
	    return 0;
	}
    }
    return 1;
}
