/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2000, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  See the "LEGAL", "LICENSE", "CREDITS" and "README" files for all the
 *  related information and legal notices. It is expected that any future
 *  projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  nav_util.c: Utilities for autonav and sail
 * 
 *  Known contributors to this file:
 *     
 */

#include "misc.h"

#include <ctype.h>
#include "var.h"
#include "ship.h"
#include "plane.h"
#include "land.h"
#include "nuke.h"
#include "sect.h"
#include "news.h"
#include "xy.h"
#include "nsc.h"
#include "nat.h"
#include "path.h"
#include "deity.h"
#include "file.h"
#include "item.h"
#include "optlist.h"
#include "player.h"
#include "update.h"
#include "subs.h"
#include "common.h"
#include "gen.h"

/* Format a ship name */
int
check_nav(struct sctstr *sect)
{
    extern struct dchrstr dchr[];

    switch (dchr[sect->sct_type].d_flg & 03) {
    case NAVOK:
	break;

    case NAV_02:
	if (sect->sct_effic < 2)
	    return CN_CONSTRUCTION;
	break;
    case NAV_60:
	if (sect->sct_effic < 60)
	    return CN_CONSTRUCTION;
	break;
    default:
	return CN_LANDLOCKED;
    }
    return CN_NAVIGABLE;
}

/* load a specific ship given its 
 * location and what field to modify.
 * new autonav code
 * Chad Zabel 6/1/94 
 */
int
load_it(register struct shpstr *sp, register struct sctstr *psect, int i)
{
    int comm, shipown, amount, ship_amt, sect_amt,
	abs_max, max_amt, transfer;
    s_char item;
    struct mchrstr *vship;

    amount = sp->shp_lend[i];
    shipown = sp->shp_own;
    item = sp->shp_tend[i];	/* commodity */
    comm = com_num(&item);

    ship_amt = getvar(comm, (s_char *)sp, EF_SHIP);
    sect_amt = getvar(comm, (s_char *)psect, EF_SECTOR);

    /* check for disloyal civilians */
    if (psect->sct_oldown != shipown && comm == V_CIVIL) {
	wu(0, shipown,
	   "Ship #%d - unable to load disloyal civilians at %s.",
	   sp->shp_uid, xyas(psect->sct_x, psect->sct_y, psect->sct_own));
	return 0;
    }
    if (comm == V_CIVIL || comm == V_MILIT)
	sect_amt--;		/* leave 1 civ or mil to hold the sector. */
    vship = &mchr[(int)sp->shp_type];
    abs_max = max_amt = (vl_find(comm, vship->m_vtype,
				 vship->m_vamt, (int)vship->m_nv));

    if (!abs_max)
	return 0;		/* can't load the ship, skip to the end. */

    max_amt = min(sect_amt, max_amt - ship_amt);
    if (max_amt <= 0 && (ship_amt != abs_max)) {
	sp->shp_autonav |= AN_LOADING;
	return 0;
    }


    transfer = amount - ship_amt;
    if (transfer > sect_amt) {	/* not enough in the   */
	transfer = sect_amt;	/* sector to fill the  */
	sp->shp_autonav |= AN_LOADING;	/* ship, set load flag */
    }
    if (ship_amt + transfer > abs_max)	/* Do not load more    */
	transfer = abs_max - ship_amt;	/* then the max alowed */
    /* on the ship.        */

    if (transfer == 0)
	return 0;		/* nothing to move */


    putvar(comm, ship_amt + transfer, (s_char *)sp, EF_SHIP);
    if (comm == V_CIVIL || comm == V_MILIT)
	sect_amt++;		/*adjustment */
    putvar(comm, sect_amt - transfer, (s_char *)psect, EF_SECTOR);

    /* deal with the plague */
    if (getvar(V_PSTAGE, (s_char *)psect, EF_SECTOR) == PLG_INFECT &&
	getvar(V_PSTAGE, (s_char *)sp, EF_SHIP) == PLG_HEALTHY)
	putvar(V_PSTAGE, PLG_EXPOSED, (s_char *)sp, EF_SHIP);
    if (getvar(V_PSTAGE, (s_char *)sp, EF_SHIP) == PLG_INFECT &&
	getvar(V_PSTAGE, (s_char *)psect, EF_SECTOR) == PLG_HEALTHY)
	putvar(V_PSTAGE, PLG_EXPOSED, (s_char *)psect, EF_SECTOR);

    return 1;			/* we did someloading return 1 to keep */
    /* our loop happy in nav_ship()        */

}

/* unload_it 
 * A guess alot of this looks like load_it but because of its location
 * in the autonav code I had to split the 2 procedures up.
 * unload_it dumps all the goods from the ship to the harbor.
 * ONLY goods in the trade fields will be unloaded.
 * new autonav code
 * Chad Zabel 6/1/94  
 */
void
unload_it(register struct shpstr *sp)
{
    struct sctstr *sectp;
    s_char item;
    int i;
    int landowner;
    int shipown;
    int comm;
    int sect_amt;
    int ship_amt;
    int abs_max = 99999;	/* max amount a sector can hold. */
    int max_amt;
    int level;


    sectp = getsectp(sp->shp_x, sp->shp_y);

    landowner = sectp->sct_own;
    shipown = sp->shp_own;

    for (i = 0; i < TMAX; ++i) {
	item = sp->shp_tend[i];
	level = sp->shp_lend[i];

	if (item == ' ' || level == 0)
	    continue;
	if (landowner == 0)
	    continue;
	if (sectp->sct_type != SCT_HARBR)
	    continue;

	comm = com_num(&item);
	ship_amt = getvar(comm, (s_char *)sp, EF_SHIP);
	sect_amt = getvar(comm, (s_char *)sectp, EF_SECTOR);

	/* check for disloyal civilians */
	if (sectp->sct_oldown != shipown && comm == V_CIVIL) {
	    wu(0, sp->shp_own,
	       "Ship #%d - unable to unload civilians into a disloyal sector at %s.",
	       sp->shp_uid, xyas(sectp->sct_x, sectp->sct_y,
				 sectp->sct_own));
	    continue;
	}
	if (comm == V_CIVIL)
	    ship_amt--;		/* This leaves 1 civs on board the ship */

	if (sect_amt >= abs_max)
	    continue;		/* The sector is full. */

	max_amt = min(ship_amt, abs_max - sect_amt);

	if (max_amt <= 0)
	    continue;

	putvar(comm, ship_amt - max_amt, (s_char *)sp, EF_SHIP);
	putvar(comm, sect_amt + max_amt, (s_char *)sectp, EF_SECTOR);

	if (getvar(V_PSTAGE, (s_char *)sectp, EF_SECTOR) == PLG_INFECT &&
	    getvar(V_PSTAGE, (s_char *)sp, EF_SHIP) == PLG_HEALTHY)
	    putvar(V_PSTAGE, PLG_EXPOSED, (s_char *)sp, EF_SHIP);

	if (getvar(V_PSTAGE, (s_char *)sp, EF_SHIP) == PLG_INFECT &&
	    getvar(V_PSTAGE, (s_char *)sectp, EF_SECTOR) == PLG_HEALTHY)
	    putvar(V_PSTAGE, PLG_EXPOSED, (s_char *)sectp, EF_SECTOR);

    }

}

/* com_num
 * This small but useful bit of code runs through the list
 * of commodities and return the integer value of the 
 * commodity it finds if possible.  Very handy when using getvar().  
 * Basicly its a hacked version of whatitem.c found in the
 * /player directory.
 * new autonav code.
 * Chad Zabel 6/1/94
 */

int
com_num(s_char *ptr)
{
    struct ichrstr *ip;

    for (ip = &ichr[1]; ip->i_mnem != 0; ip++) {
	if (*ptr == ip->i_mnem)
	    return ip->i_vtype;
    }
    return 0;			/*NOTREACHED*/
}



/* auto_fuel_ship 
 * Assume a check for fuel=0 has already been made and passed.  
 * Try to fill a ship using petro. and then oil.            
 * new autonav code.
 * This should be merged with the fuel command someday. 
 * Chad Zabel 6/1/94
 */

void
auto_fuel_ship(register struct shpstr *sp)
{
    double d;
    int totalfuel = 0;
    int need;
    int maxfuel;
    int newfuel = 0;
    int add_fuel = 0;

    if (opt_FUEL == 0)
	return;
    getship(sp->shp_uid, sp);	/* refresh */
    /* fill with petro */
    maxfuel = mchr[(int)sp->shp_type].m_fuelc;
    d = (double)maxfuel / 5.0;
    if ((d - (int)d > 0.0))
	d++;
    need = (int)d;

    newfuel = supply_commod(sp->shp_own, sp->shp_x,
			    sp->shp_y, I_PETROL, need);
    add_fuel += newfuel * 5;
    if (add_fuel > maxfuel)
	add_fuel = maxfuel;
    sp->shp_fuel += add_fuel;
    totalfuel += add_fuel;

    if (totalfuel == maxfuel) {
	putship(sp->shp_uid, sp);
	return;			/* the ship is full */
    }
    add_fuel = 0;
    /* fill with oil */
    d = (double)(maxfuel - totalfuel) / 50.0;
    if ((d - (int)d > 0.0))
	d++;
    need = (int)d;

    newfuel = supply_commod(sp->shp_own, sp->shp_x,
			    sp->shp_y, I_OIL, need);
    add_fuel = newfuel * 50;
    if (add_fuel > maxfuel)
	add_fuel = maxfuel;
    sp->shp_fuel += add_fuel;
    putship(sp->shp_uid, sp);
}
