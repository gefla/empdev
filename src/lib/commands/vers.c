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
 *  vers.c: Print out the Empire version
 * 
 *  Known contributors to this file:
 *     Dave Pare
 *     Jeff Bailey
 *     Thomas Ruschak
 *     Ken Stevens
 *     Steve McClure
 */

#include <stdio.h>
#include <time.h>
#include "gamesdef.h"
#include "misc.h"
#include "player.h"
#include "deity.h"
#include "nat.h"
#include "version.h"
#include "ship.h"
#include "optlist.h"
#include "commands.h"

extern float drnuke_const;

int
vers(void)
{
    extern int s_p_etu;
    extern int etu_per_update;
    extern int m_m_p_d;
    extern int players_at_00;
    extern float btu_build_rate;
    extern double fgrate, fcrate;
    extern double eatrate, babyeat;
    extern double obrate, uwbrate;
    extern double bankint;
    extern double hap_cons, edu_cons;
    extern double money_civ, money_uw, money_mil, money_res;
    extern float hap_avg, edu_avg, ally_factor;
    extern float level_age_rate;
/*	extern	float easy_tech, hard_tech, tech_log_base; */
    extern float easy_tech, tech_log_base;
    extern int land_mob_max;
    extern int land_grow_scale;
    extern float land_mob_scale;
    extern int sect_mob_max;
    extern float sect_mob_scale;
    extern int ship_mob_max;
    extern float ship_mob_scale;
    extern int ship_grow_scale;
    extern int plane_mob_max;
    extern float plane_mob_scale;
    extern int plane_grow_scale;
    extern int War_Cost;
    extern float fire_range_factor;
    extern int trade_1_dist;	/* less than this gets no money */
    extern int trade_2_dist;	/* less than this gets trade_1 money */
    extern int trade_3_dist;	/* less than this gets trade_2 money */
    extern float trade_1;	/* return on trade_1 distance */
    extern float trade_2;	/* return on trade_2 distance */
    extern float trade_3;	/* return on trade_3 distance */
    extern float trade_ally_bonus;	/* 20% bonus for trading with allies */
    extern float trade_ally_cut;	/* 10% bonus for ally you trade with */
    extern double tradetax;
    extern double buytax;
    struct option_list *op;

    time_t now;
    int j;

    (void)time(&now);
    pr("Empire %d.%d.%d\n(KSU distribution %2.2f, Chainsaw version %2.2f, Wolfpack version %2.2f)\n\n", EMP_VERS_MAJOR, EMP_VERS_MINOR, EMP_VERS_PATCH, (float)KSU_DIST, (float)CHAINSAW_DIST, (float)WOLFPACK_DIST);
    pr("The following parameters have been set for this game:\n");
    pr("World size is %d by %d.\n", WORLD_X, WORLD_Y);
    pr("There can be up to %d countries.\n", MAXNOC);
    pr("By default, countries use %s coordinate system.\n",
       (players_at_00) ? "the deity's" : "their own");
    pr("\n");
    pr("An Empire time unit is %d second%s long.\n",
       s_p_etu, s_p_etu != 1 ? "s" : "");
    pr("Use the 'update' command to find out the time of the next update.\n");
    pr("The current time is %19.19s.\n", ctime(&now));
    pr("An update consists of %d empire time units.\n", etu_per_update);
    pr("Each country is allowed to be logged in %d minutes a day.\n",
       m_m_p_d);
    pr("It takes %.2f civilians to produce a BTU in one time unit.\n",
       (1.0 / (btu_build_rate * 100.0)));
    pr("\n");

    pr("A non-aggi, 100 fertility sector can grow %.2f food per etu.\n",
       100.0 * fgrate);
    pr("1000 civilians will harvest %.1f food per etu.\n",
       1000.0 * fcrate);
    pr("1000 civilians will give birth to %.1f babies per etu.\n",
       1000.0 * obrate);
    pr("1000 uncompensated workers will give birth to %.1f babies.\n",
       1000.0 * uwbrate);
    pr("In one time unit, 1000 people eat %.1f units of food.\n",
       1000.0 * eatrate);
    pr("1000 babies eat %.1f units of food becoming adults.\n",
       1000.0 * babyeat);
    if (opt_NOFOOD)
	pr("No food is needed!!\n");

    pr("\n");

    pr("Banks pay $%.2f in interest per 1000 gold bars per etu.\n",
       bankint * 1000.0);
    pr("1000 civilians generate $%.2f, uncompensated workers $%.2f each time unit.\n", 1000.0 * money_civ, 1000.0 * money_uw);
    pr("1000 active military cost $%.2f, reserves cost $%.2f.\n",
       -money_mil * 1000.0, -money_res * 1000.0);
    if (opt_SLOW_WAR)
	pr("Declaring war will cost you $%i\n\n", War_Cost);
    pr("Happiness p.e. requires 1 happy stroller per %d civ.\n",
       (int)hap_cons / etu_per_update);
    pr("Education p.e. requires 1 class of graduates per %d civ.\n",
       (int)edu_cons / etu_per_update);
    pr("Happiness is averaged over %d time units.\n", (int)hap_avg);
    pr("Education is averaged over %d time units.\n", (int)edu_avg);
    if (opt_ALL_BLEED == 0)
	pr("The technology/research boost you get from your allies is %.2f%%.\n", 100.0 / ally_factor);
    else			/* ! ALL_BLEED */
	pr("The technology/research boost you get from the world is %.2f%%.\n", 100.0 / ally_factor);

    pr("Nation levels (tech etc.) decline 1%% every %d time units.\n",
       (int)(level_age_rate));

    pr("Tech Buildup is ");
/*	if (tech_log_base <= 1.0 && hard_tech == 0.0) { */
    if (tech_log_base <= 1.0) {
	pr("not limited\n");
    }
    if (tech_log_base > 1.0) {
	pr("limited to logarithmic growth (base %.2f)", tech_log_base);
	if (easy_tech == 0.0)
	    pr(".\n");
	else
	    pr(" after %0.2f.\n", easy_tech);
    }
    /*else {
       pr("limited to asymptotic growth towards %.2f",
       hard_tech + easy_tech);
       if (easy_tech == 0.00) 
       pr(".\n");
       else
       pr("after %.2f\n",easy_tech);
       } */
    pr("\n");
    pr("\t\t\t\tSectors\tShips\tPlanes\tUnits\n");
    pr("Maximum mobility\t\t%d\t%d\t%d\t%d\n", sect_mob_max,
       ship_mob_max, plane_mob_max, land_mob_max);
    pr("Max mob gain per update\t\t%d\t%d\t%d\t%d\n",
       (int)(sect_mob_scale * (float)etu_per_update),
       (int)(ship_mob_scale * (float)etu_per_update),
       (int)(plane_mob_scale * (float)etu_per_update),
       (int)(land_mob_scale * (float)etu_per_update));
    pr("Max eff gain per update\t\t--\t%d\t%d\t%d\n",
       min(ship_grow_scale * etu_per_update, 100),
       min(plane_grow_scale * etu_per_update, 100),
       min(land_grow_scale * etu_per_update, 100));
    pr("\n");
    pr("Ships on autonavigation may use %i cargo holds per ship.\n", TMAX);
    if (opt_TRADESHIPS) {
	pr("Trade-ships that go at least %d sectors get a return of %.1f%% per sector.\n", trade_1_dist, (float)(trade_1 * 100.0));
	pr("Trade-ships that go at least %d sectors get a return of %.1f%% per sector.\n", trade_2_dist, (float)(trade_2 * 100.0));
	pr("Trade-ships that go at least %d sectors get a return of %.1f%% per sector.\n", trade_3_dist, (float)(trade_3 * 100.0));
	pr("Cashing in trade-ships with an ally nets you a %.1f%% bonus.\n", trade_ally_bonus * 100.0);
	pr("Cashing in trade-ships with an ally nets your ally a %.1f%% bonus.\n\n", trade_ally_cut * 100.0);
    }
    if (opt_MARKET) {
	pr("The tax you pay on selling things on the trading block is %.1f%%\n", (1.00 - tradetax) * 100.0);
	pr("The tax you pay on buying commodities on the market is %.1f%%\n\n", (buytax - 1.00) * 100.0);
    }

    if (opt_NONUKES)
	pr("Nukes are disabled.\n");
    else if (opt_DRNUKE) {	/* NUKES && DRNUKE enabled */
	pr("In order to build a nuke, you need %1.2f times the tech level in research\n", drnuke_const);
	pr("\tExample: In order to build a 300 tech nuke, you need %d research\n\n", (int)(300.0 * drnuke_const));
    }

    pr("Fire ranges are scaled by %.2f\n", fire_range_factor);

    pr("\nOptions enabled in this game:\n        ");
    for (j = 0, op = Options; op->opt_key; op++) {
	if (*op->opt_valuep == 0)
	    continue;

	j += strlen(op->opt_key) + 2;
	if (j > 70) {
	    pr("\n        ");
	    j = strlen(op->opt_key) + 2;
	}
	pr("%s%s", op->opt_key, op[1].opt_key == NULL ? "" : ", ");
    }
    pr("\n\nOptions disabled in this game:\n        ");
    for (j = 0, op = Options; op->opt_key; op++) {
	if (!(*op->opt_valuep == 0))
	    continue;

	j += strlen(op->opt_key) + 2;
	if (j > 70) {
	    pr("\n        ");
	    j = strlen(op->opt_key) + 2;
	}
	pr("%s%s", op->opt_key, op[1].opt_key == NULL ? "" : ", ");
    }
    pr("\n\n\"info Options\" for a detailed list of options and descriptions");
    pr("\n\n");
    pr("The person to annoy if something goes wrong is:\n\t%s\n\t(%s).\n",
       privname, privlog);
    pr("You can get your own copy of the source %s\n", GET_SOURCE);
    return RET_OK;
}
