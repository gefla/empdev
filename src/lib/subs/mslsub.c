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
 *  mslsub.c: Missile subroutine stuff
 * 
 *  Known contributors to this file:
 *     Ken Stevens, 1995
 *     Steve McClure, 1996-2000
 */

#include "misc.h"
#include "queue.h"
#include "player.h"
#include "var.h"
#include "sect.h"
#include "ship.h"
#include "optlist.h"
#include "nuke.h"
#include "plane.h"
#include "land.h"
#include "news.h"
#include "item.h"
#include "xy.h"
#include "nsc.h"
#include "file.h"
#include "nat.h"
#include "path.h"
#include "mission.h"
#include "prototypes.h"

int
msl_equip(struct plnstr *pp)
{
    struct plist pl;

    bzero((s_char *)&pl, sizeof(struct plist));
    pl.pcp = plchr + pp->pln_type;
    bcopy((s_char *)pp, (s_char *)&pl.plane, sizeof(struct plnstr));
    return mission_pln_equip(&pl, 0, 0, 'p');
}

int
msl_hit(struct plnstr *pp, int hardtarget, int type, int news_item,
	int snews_item, s_char *what, coord x, coord y, int victim)
{
    int hit;
    struct shpstr ship;
    struct sctstr sect;
    int sublaunch = 0;
    struct plchrstr *pcp = plchr + pp->pln_type;
    int hitchance = pln_hitchance(pp, hardtarget, type);
    s_char *from;
    int dam, dummyi;

    mpr(pp->pln_own, "Preparing to launch %s at %s %s %s%s\n",
	prplane(pp),
	cname(victim),
	what,
	(type == EF_SHIP || type == EF_PLANE) ? "in " : "",
	xyas(x, y, pp->pln_own));
    mpr(pp->pln_own, "\tLaunching from ");
    if (pp->pln_ship >= 0) {
	getship(pp->pln_ship, &ship);
	mpr(pp->pln_own, "%s in ", prship(&ship));
	if (mchr[(int)ship.shp_type].m_flags & M_SUB) {
	    sublaunch = 1;
	    from = "in hatch";
	} else
	    from = "on deck";
	mpr(pp->pln_own, "%s\n",
	    xyas(ship.shp_x, ship.shp_y, pp->pln_own));
    } else {
	if (pp->pln_harden > 0) {
	    mpr(pp->pln_own, "missile silo at ");
	    from = "in cilo";
	} else
	    from = "on launch pad";
	mpr(pp->pln_own, "%s\n", xyas(pp->pln_x, pp->pln_y, pp->pln_own));
    }

    if (chance((0.05 + (100 - pp->pln_effic) / 100.0)
	       * (1 - techfact(pp->pln_tech, 1.0)))) {
	mpr(pp->pln_own, "KABOOOOM!  Missile explodes %s!\n", from);
	if (chance(0.33)) {
	    if (pp->pln_nuketype != (s_char)-1 && opt_NUKEFAILDETONATE) {
		pp->pln_flags &= ~PLN_AIRBURST;
		detonate(pp, pp->pln_x, pp->pln_y);
	    } else {
		dam = pln_damage(pp, pp->pln_x, pp->pln_y,
				 'p', &dummyi, 1) / 2;
		if (dam) {
		    if (pp->pln_ship >= 0) {
			shipdamage(&ship, dam);
			putship(ship.shp_uid, &ship);
		    } else {
			pr("Explosion damages %s %d%%",
			   xyas(pp->pln_x, pp->pln_y, pp->pln_own), dam);
			getsect(pp->pln_x, pp->pln_y, &sect);
			sectdamage(&sect, dam, 0);
			putsect(&sect);
		    }
		}
	    }
	}
	return 0;
    }

    mpr(pp->pln_own, "\tSHWOOOOOSH!  Missile launched!\n");

    if (pp->pln_nuketype != (s_char)-1)
	mpr(pp->pln_own, "\tArming nuclear warheads...\n");

    if (pcp->pl_flags & P_T)
	mpr(victim, "Incoming %s missile...\n",
	    sublaunch ? (s_char *)"" : cname(pp->pln_own));

    if (opt_PINPOINTMISSILE == 0 ||
	(pcp->pl_flags & P_T && !(pcp->pl_flags & P_MAR))) {
	if (msl_intercept(x, y, pp->pln_own, pcp->pl_def,
			  sublaunch, P_N, P_O)) {
	    return 0;
	}
    }
    if (pcp->pl_flags & P_MAR) {
	if (shp_missile_defense(x, y, pp->pln_own, pcp->pl_def)) {
	    return 0;
	}
    }

    if (pp->pln_nuketype != (s_char)-1)
	hitchance = 100;

    mpr(pp->pln_own, "\t%d%% hitchance...", hitchance);
    hit = (roll(100) <= hitchance);

    mpr(pp->pln_own, hit ? "HIT!\n" : "miss\n");
    if (pcp->pl_flags & P_T)
	mpr(victim, "...Incoming %s missile %s\n",
	    sublaunch ? (s_char *)"" : cname(pp->pln_own),
	    hit ? "HIT!\n" : "missed\n");
    if (hit && news_item) {
	if (sublaunch)
	    nreport(victim, snews_item, 0, 1);
	else
	    nreport(pp->pln_own, news_item, victim, 1);
    }
    return hit;
}

void
msl_sel(struct emp_qelem *list, coord x, coord y, natid victim,
	int wantflags, int nowantflags, int mission)
{
    register struct plchrstr *pcp;
    struct plnstr plane;
    struct plist *irv;
    struct nstr_item ni;

    emp_initque(list);
    snxtitem_all(&ni, EF_PLANE);
    while (nxtitem(&ni, (s_char *)&plane)) {
	if (!plane.pln_own)
	    continue;

	pcp = &plchr[(int)plane.pln_type];
	if (!(pcp->pl_flags & P_M))
	    continue;
	if (wantflags && (pcp->pl_flags & wantflags) != wantflags)
	    continue;
	if (nowantflags && pcp->pl_flags & nowantflags)
	    continue;
	if (mission && plane.pln_mission != mission)
	    continue;
	if (getrel(getnatp(plane.pln_own), victim) >= NEUTRAL)
	    continue;
	/* missiles go one way, so we can use all the range */
	if (plane.pln_range < mapdist(x, y, plane.pln_x, plane.pln_y))
	    continue;
	if (plane.pln_mobil <= (s_char)0)
	    continue;
	if (plane.pln_effic < 100)
	    continue;
	/* got a valid interceptor */
	irv = (struct plist *)malloc(sizeof(*irv));
	irv->state = P_OK;
	irv->bombs = 0;
	irv->misc = 0;
	irv->pcp = &plchr[(int)plane.pln_type];
	bcopy((s_char *)&plane, (s_char *)&irv->plane, sizeof(plane));
	emp_insque(&irv->queue, list);
    }
}

int
msl_intercept(coord x, coord y, natid bombown, int hardtarget,
	      int sublaunch, int wantflags, int nowantflags)
{
    register struct plnstr *pp;
    register struct plchrstr *pcp;
    struct sctstr sect;
    struct emp_qelem *irvlist;
    struct emp_qelem foo;
    struct emp_qelem *intlist;
    struct emp_qelem intfoo;
    struct emp_qelem *qp;
    struct emp_qelem *next;
    struct plist *ip;
    int icount = 0;
    short destroyed = 0;
    s_char *att_name;
    s_char *def_name;
    int news_item;
    s_char what[512];
    s_char *who = sublaunch ? (s_char *)"" : cname(bombown);

    getsect(x, y, &sect);
    if (wantflags == P_O && !nowantflags) {
	att_name = "satellite";
	def_name = "a-sat missile";
	news_item = N_SAT_KILL;
	if (sect.sct_own) {
	    mpr(sect.sct_own, "%s has positioned a satellite over %s\n",
		sublaunch ? (s_char *)"someone" : cname(bombown), xyas(x,
								       y,
								       sect.
								       sct_own));
	}
    } else if (wantflags == P_N && nowantflags == P_O) {
	att_name = "warhead";
	def_name = "abm";
	news_item = sublaunch ? N_NUKE_SSTOP : N_NUKE_STOP;
    } else {
	att_name = "elephant";
	def_name = "tomato";	/* heh -KHS */
	news_item = N_NUKE_STOP;
    }
    sprintf(what, "%s %s", who, att_name);
    irvlist = &foo;

    /* get all hostile abms in range */
    msl_sel(irvlist, x, y, bombown, wantflags, nowantflags, 0);
    intlist = &intfoo;
    emp_initque(intlist);
    /* First choose interceptors belonging to the target sector */
    /* only allow two defense missiles per missile attack */
    for (qp = irvlist->q_forw; qp != irvlist && icount < 2; qp = next) {
	next = qp->q_forw;
	ip = (struct plist *)qp;
	pp = &ip->plane;
	if (pp->pln_own != sect.sct_own)
	    continue;
	pcp = ip->pcp;
	if (mission_pln_equip(ip, 0, 0, 'i') < 0) {
	    emp_remque(qp);
	    free((s_char *)qp);
	    continue;
	}
	/* got one interceptor, delete from irv_list and
	 * add to  int_list.
	 */
	emp_remque(qp);
	emp_insque(qp, intlist);
	putplane(pp->pln_uid, pp);
	icount++;
    }
    /* only allow two defense missiles per missile attack */
    for (qp = irvlist->q_forw; qp != irvlist && icount < 2; qp = next) {
	next = qp->q_forw;
	ip = (struct plist *)qp;
	pp = &ip->plane;
	pcp = ip->pcp;
	if (mission_pln_equip(ip, 0, 0, 'i') < 0) {
	    emp_remque(qp);
	    free((s_char *)qp);
	    continue;
	}
	/* got one interceptor, delete from irv_list and
	 * add to  int_list.
	 */
	emp_remque(qp);
	emp_insque(qp, intlist);
	putplane(pp->pln_uid, pp);
	icount++;
    }
    /* Now, clean out the queue */
    while (!QEMPTY(irvlist)) {
	qp = irvlist->q_forw;
	emp_remque(qp);
	free((s_char *)qp);
    }
    if (icount == 0) {
	if (sect.sct_own != 0)
	    mpr(sect.sct_own, "No %ss launched to intercept.\n", def_name);
	return (destroyed);
    }

    /* attempt to destroy incoming missile */

    while (!QEMPTY(intlist)) {
	qp = intlist->q_forw;
	ip = (struct plist *)qp;
	pp = &ip->plane;
	pcp = ip->pcp;

	mpr(bombown, "%s %s launched in defense!\n",
	    cname(pp->pln_own), def_name);
	if (sect.sct_own == pp->pln_own) {
	    mpr(sect.sct_own, "%s launched to intercept %s %s!\n",
		def_name, who, att_name);
	} else {
	    if (sect.sct_own)
		mpr(sect.sct_own,
		    "%s launched an %s to intercept the %s %s!\n",
		    cname(pp->pln_own), def_name, who, att_name);
	    mpr(pp->pln_own,
		"%s launched to intercept %s %s arcing towards %s territory!\n",
		def_name, who, att_name, cname(sect.sct_own));
	}

	if (!destroyed &&
	    msl_hit(pp, hardtarget, EF_PLANE, news_item, news_item, what,
		    x, y, bombown)) {
	    mpr(bombown, "%s destroyed by %s %s!\n", att_name,
		cname(pp->pln_own), def_name);
	    if (sect.sct_own)
		mpr(sect.sct_own, "%s %s intercepted!\n", who, att_name);
	    if (sect.sct_own != pp->pln_own)
		mpr(pp->pln_own, "%s %s intercepted!\n", who, att_name);
	    destroyed = 1;
	}
	/* zap the missile */
	makelost(EF_PLANE, pp->pln_own, pp->pln_uid, pp->pln_x, pp->pln_y);
	pp->pln_own = 0;
	putplane(pp->pln_uid, pp);
	emp_remque(qp);
	free((s_char *)qp);
	if (destroyed)
	    break;
    }
    /* Clean out what is left in the list */
    while (!QEMPTY(intlist)) {
	qp = intlist->q_forw;
	emp_remque(qp);
	free((s_char *)qp);
    }
    if (destroyed)
	return (destroyed);
    if (icount) {
	mpr(bombown, "%s made it through %s defenses!\n", att_name,
	    def_name);
	if (sect.sct_own)
	    mpr(sect.sct_own, "%s made it through %s defenses!\n",
		att_name, def_name);
    }
    return (destroyed);
}

/* Keep launching missiles on list until mindam damage has been done */
int
msl_launch_mindam(struct emp_qelem *list, coord x, coord y, int hardtarget,
		  int type, int mindam, s_char *whatp, int victim,
		  int mission)
{
    struct emp_qelem *qp;
    struct emp_qelem *next;
    struct plist *plp;
    int newdam, dam = 0;
    int nukedam = 0;
    int news_item;
    int snews_item;

    if (type == EF_SHIP) {
	news_item = N_SHP_MISS;
	snews_item = N_SHP_SMISS;
    } else if (type == EF_LAND) {
	news_item = N_LND_MISS;
	snews_item = N_LND_SMISS;
    } else {
	news_item = N_SCT_MISS;
	snews_item = N_SCT_SMISS;
    }

    for (qp = list->q_back; qp != list && dam < mindam && !nukedam;
	 qp = next) {
	next = qp->q_back;
	plp = (struct plist *)qp;

	if (mission_pln_equip(plp, 0, 0, 'p') >= 0) {
	    if (msl_hit(&plp->plane,
			hardtarget, type, news_item, snews_item,
			whatp, x, y, victim)) {
		newdam = pln_damage(&plp->plane, x, y, 'p', &nukedam, 1);
		if (nukedam) {
		    if (mission == MI_INTERDICT && type == EF_SECTOR)
			dam += nukedam;
		} else
		    dam += newdam;
	    } else {
		/* Missiles that miss have to hit somewhere! */
		newdam = pln_damage(&plp->plane, x, y, 'p', &nukedam, 0);
		collateral_damage(x, y, newdam, 0);
	    }
	    makelost(EF_PLANE, plp->plane.pln_own, plp->plane.pln_uid,
		     plp->plane.pln_x, plp->plane.pln_y);
	    plp->plane.pln_own = 0;
	    putplane(plp->plane.pln_uid, &plp->plane);
	    emp_remque(qp);
	    free(qp);
	}
    }
    return dam;
}
