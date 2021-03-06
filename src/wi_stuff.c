/*
====================================================================

DOOM RETRO
A classic, refined DOOM source port. For Windows PC.

Copyright � 1993-1996 id Software LLC, a ZeniMax Media company.
Copyright � 2005-2014 Simon Howard.
Copyright � 2013-2014 Brad Harding.

This file is part of DOOM RETRO.

DOOM RETRO is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DOOM RETRO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with DOOM RETRO. If not, see http://www.gnu.org/licenses/.

====================================================================
*/

#include <stdio.h>

#include "SDL.h"

#include "z_zone.h"

#include "m_random.h"
#include "i_swap.h"

#include "i_system.h"

#include "w_wad.h"

#include "g_game.h"

#include "r_local.h"
#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

// Needs access to LFB.
#include "v_video.h"

#include "wi_stuff.h"

#include "v_data.h"

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//


//
// Difference between registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
#define NUMEPISODES             4
#define NUMMAPS                 9


// GLOBAL LOCATIONS
#define WI_TITLEY               2
#define WI_SPACINGY             33

// SINGLE-PLAYER STUFF
#define SP_STATSX               50
#define SP_STATSY               50

#define SP_TIMEX                16
#define SP_TIMEY                (ORIGINALHEIGHT - 32)


// NET GAME STUFF
#define NG_STATSY               50
#define NG_STATSX               (32 + SHORT(star->width) / 2 + 32 * !dofrags)

#define NG_SPACINGX             64


// DEATHMATCH STUFF
#define DM_MATRIXX              42
#define DM_MATRIXY              68

#define DM_SPACINGX             40

#define DM_TOTALSX              269

#define DM_KILLERSX             10
#define DM_KILLERSY             100
#define DM_VICTIMSX             5
#define DM_VICTIMSY             50




typedef enum
{
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_LEVEL

} animenum_t;

typedef struct
{
    int                 x;
    int                 y;

} point_t;


//
// Animation.
// There is another anim_t used in p_spec.
//
typedef struct
{
    animenum_t          type;

    // period in tics between animations
    int                 period;

    // number of animation frames
    int                 nanims;

    // location of animation
    point_t             loc;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    int                 data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    int                 data2;

    // actual graphics for frames of animations
    patch_t             *p[3];

    // following must be initialized to zero before use!

    // next value of bcnt (used in conjunction with period)
    int                 nexttic;

    // last drawn animation frame
    int                 lastdrawn;

    // next frame number to animate
    int                 ctr;

    // used by RANDOM and LEVEL when animating
    int                 state;

} anim_t;


static point_t lnodes[NUMEPISODES][NUMMAPS] =
{
    // Episode 0 World Map
    {
        { 185, 164 },   // location of level 0 (CJ)
        { 148, 143 },   // location of level 1 (CJ)
        {  69, 122 },   // location of level 2 (CJ)
        { 209, 102 },   // location of level 3 (CJ)
        { 116,  89 },   // location of level 4 (CJ)
        { 166,  55 },   // location of level 5 (CJ)
        {  71,  56 },   // location of level 6 (CJ)
        { 135,  29 },   // location of level 7 (CJ)
        {  71,  24 }    // location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
        { 254,  25 },   // location of level 0 (CJ)
        {  97,  50 },   // location of level 1 (CJ)
        { 188,  64 },   // location of level 2 (CJ)
        { 128,  78 },   // location of level 3 (CJ)
        { 214,  92 },   // location of level 4 (CJ)
        { 133, 130 },   // location of level 5 (CJ)
        { 208, 136 },   // location of level 6 (CJ)
        { 148, 140 },   // location of level 7 (CJ)
        { 235, 158 }    // location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
        { 156, 168 },   // location of level 0 (CJ)
        {  48, 154 },   // location of level 1 (CJ)
        { 174,  95 },   // location of level 2 (CJ)
        { 265,  75 },   // location of level 3 (CJ)
        { 130,  48 },   // location of level 4 (CJ)
        { 279,  23 },   // location of level 5 (CJ)
        { 198,  48 },   // location of level 6 (CJ)
        { 140,  25 },   // location of level 7 (CJ)
        { 281, 136 }    // location of level 8 (CJ)
    }

};


//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//

#define ANIM(type, period, nanims, x, y, nexttic)                  \
            { (type), (period), (nanims), { (x), (y) }, (nexttic), \
              0, { NULL, NULL, NULL }, 0, 0, 0, 0 }


static anim_t epsd0animinfo[] =
{
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 224, 104, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 184, 160, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 112, 136, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  72, 112, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  88,  96, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  64,  48, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 192,  40, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 136,  16, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  80,  16, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  64,  24, 0),
};

static anim_t epsd1animinfo[] =
{
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 1),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 2),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 3),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 4),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 5),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 6),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 7),
    ANIM(ANIM_LEVEL, TICRATE / 3, 3, 192, 144, 8),
    ANIM(ANIM_LEVEL, TICRATE / 3, 1, 128, 136, 8),
};

static anim_t epsd2animinfo[] =
{
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 104, 168, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3,  40, 136, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 160,  96, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 104,  80, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 3, 3, 120,  32, 0),
    ANIM(ANIM_ALWAYS, TICRATE / 4, 3,  40,   0, 0),
};

static int NUMANIMS[NUMEPISODES] =
{
    arrlen(epsd0animinfo),
    arrlen(epsd1animinfo),
    arrlen(epsd2animinfo),
};

static anim_t *anims[NUMEPISODES] =
{
    epsd0animinfo,
    epsd1animinfo,
    epsd2animinfo
};


//
// GENERAL DATA
//

//
// Locally used stuff.
//
#define FB                      0


// States for single-player
#define SP_KILLS                0
#define SP_ITEMS                2
#define SP_SECRET               4
#define SP_FRAGS                6
#define SP_TIME                 8
#define SP_PAR                  ST_TIME

#define SP_PAUSE                1

// in seconds
#define SHOWNEXTLOCDELAY        4


// used to accelerate or skip a stage
static int              acceleratestage;

// wbs->pnum
static int              me;

// specifies current state
static stateenum_t      state;

// contains information passed into intermission
static wbstartstruct_t  *wbs;

static wbplayerstruct_t *plrs;  // wbs->plyr[]

// used for general timing
static int              cnt;

// used for timing of background animation
static int              bcnt;

// signals to refresh everything for one frame
static int              firstrefresh;

static int              cnt_kills[MAXPLAYERS];
static int              cnt_items[MAXPLAYERS];
static int              cnt_secret[MAXPLAYERS];
static int              cnt_time;
static int              cnt_par;
static int              cnt_pause;

// # of commercial levels
static int              NUMCMAPS;


//
//      GRAPHICS
//

// You Are Here graphic
static patch_t          *yah[3] = { NULL, NULL, NULL };

// splat
static patch_t          *splat[2] = { NULL, NULL };

// %, : graphics
static patch_t          *percent;
static patch_t          *colon;

// 0-9 graphic
static patch_t          *num[10];

// minus sign
static patch_t          *wiminus;

// "Finished!" graphics
static patch_t          *finished;

// "Entering" graphic
static patch_t          *entering;

// "secret"
static patch_t          *sp_secret;

// "Kills", "Scrt", "Items", "Frags"
static patch_t          *kills;
static patch_t          *secret;
static patch_t          *items;
static patch_t          *frags;

// Time sucks.
static patch_t          *timepatch;
static patch_t          *par;
static patch_t          *sucks;

// "killers", "victims"
static patch_t          *killers;
static patch_t          *victims;

// "Total", your face, your dead face
static patch_t          *total;
static patch_t          *star;
static patch_t          *bstar;

// "red P[1..MAXPLAYERS]"
static patch_t          *p[MAXPLAYERS];

// "gray P[1..MAXPLAYERS]"
static patch_t          *bp[MAXPLAYERS];

// Name graphics of each level (centered)
static patch_t          **lnames;

extern byte *tinttab50;

//
// CODE
//

// slam background
void WI_slamBackground(void)
{
    memcpy(screens[0], screens[1], SCREENWIDTH * SCREENHEIGHT);
}

// [BH] Draws character of "<Levelname>"
void WI_drawWILVchar(int x, int y, int i)
{
    int w, x1, y1;

    w = strlen(wilv[i]) / 13;

    for (y1 = 0; y1 < 13; y1++)
        for (x1 = 0; x1 < w; x1++)
            V_DrawPixel(x + x1, y + y1, 0, (int)wilv[i][y1 * w + x1], true);
}

char            *mapname = "";
char            *nextmapname = "";

extern char *mapnames[][6];

int chartoi[130] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 000 - 009
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 010 - 019
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 020 - 029
    -1, -1, -1, 27, -1, -1, -1, -1, -1, 28, // 030 - 039
    -1, -1, -1, -1, -1, 29, 26, 30, 31, 32, // 040 - 049
    33, 34, 35, 36, 37, 38, 39, 40, -1, -1, // 050 - 059
    -1, -1, -1, -1, -1,  0,  1,  2,  3,  4, // 060 - 069
     5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 070 - 079
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 080 - 089
    25, -1, -1, -1, -1, -1, -1,  0,  1,  2, // 090 - 099
     3,  4,  5,  6,  7,  8,  9, 10, 11, 12, // 100 - 109
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, // 110 - 119
    23, 24, 25, -1, -1, -1, -1, -1, -1, -1, // 120 - 129
};

void WI_drawWILV(int y, char *str)
{
    int i, w = 0, x;

    for (i = 0; (unsigned)i < strlen(str); i++)
    {
        int j = chartoi[str[i]];

        w += (j == -1 ? 6 : (strlen(wilv[j]) / 13 - 2));
    }
    x = (ORIGINALWIDTH - w - 1) / 2;

    for (i = 0; (unsigned)i < strlen(str); i++)
    {
        int j = chartoi[str[i]];

        if (str[i] == '\'' && (!i || (i > 0 && str[i - 1] == ' ')))
            j = 41;

        if (j == -1)
        {
            x += 6;
        }
        else
        {
            WI_drawWILVchar(x, y, j);
            x += strlen(wilv[j]) / 13 - 2;
        }
    }
}

// Draws "<Levelname> Finished!"
void WI_drawLF(void)
{
    int x = (ORIGINALWIDTH - SHORT(finished->width)) / 2;
    int y = WI_TITLEY;

    // draw <LevelName>
    char name[9];
    if (gamemode == commercial)
        snprintf(name, 9, "CWILV%2.2d", wbs->last);
    else
        snprintf(name, 9, "WILV%d%d", wbs->epsd, wbs->last);
    if (W_CheckMultipleLumps(name) > 1 && !nerve)
    {
        V_DrawPatchWithShadow((ORIGINALWIDTH - SHORT(lnames[wbs->last]->width)) / 2 + 1, y + 1,
                              FB, lnames[wbs->last], false);
    }
    else
    {
        WI_drawWILV(y, mapname);
    }

    // draw "Finished!"
    y += 14;
    V_DrawPatchWithShadow(x + 1, y + 1, FB, finished, false);
}

// Draws "Entering <LevelName>"
void WI_drawEL(void)
{
    int x = (ORIGINALWIDTH - SHORT(entering->width)) / 2;
    int y = WI_TITLEY;
    char name[9];

    // draw "Entering"
    V_DrawPatchWithShadow(x + 1, y + 1, FB, entering, false);

    // draw level
    y += 14;
    if (gamemode == commercial)
        snprintf(name, 9, "CWILV%2.2d", wbs->next);
    else
        snprintf(name, 9, "WILV%d%d", wbs->epsd, wbs->next);
    if (W_CheckMultipleLumps(name) > 1 && !nerve)
    {
        V_DrawPatchWithShadow((ORIGINALWIDTH - SHORT(lnames[wbs->next]->width)) / 2 + 1, y + 1,
                              FB, lnames[wbs->next], false);
    }
    else
    {
        WI_drawWILV(y, nextmapname);
    }
}

void WI_drawOnLnode(int n, patch_t *c[])
{
    boolean     fits = false;
    int         i = 0;

    do
    {
        int     left = lnodes[wbs->epsd][n].x - SHORT(c[i]->leftoffset);
        int     top = lnodes[wbs->epsd][n].y - SHORT(c[i]->topoffset);
        int     right = left + SHORT(c[i]->width);
        int     bottom = top + SHORT(c[i]->height);

        if (left >= 0 && right < ORIGINALWIDTH && top >= 0 && bottom < ORIGINALHEIGHT)
            fits = true;
        else
            i++;
    } while (!fits && i != 2 && c[i] != NULL);

    if (fits && i < 2)
    {
        if (c[i] == *splat)
        {
            V_DrawTranslucentNoGreenPatch(lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y,
                                          FB, c[i]);
        }
        else if (c[i] == *yah)
        {
            V_DrawPatchNoGreenWithShadow(lnodes[wbs->epsd][n].x + 1, lnodes[wbs->epsd][n].y + 1,
                                         FB, c[i]);
        }
        else
        {
            V_DrawPatch(lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y, FB, c[i]);
        }
    }
}



void WI_initAnimatedBack(void)
{
    int         i;
    anim_t      *a;

    if (gamemode == commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];

        // init variables
        a->ctr = -1;

        // specify the next time to draw it
        if (a->type == ANIM_ALWAYS)
            a->nexttic = bcnt + 1 + (M_Random() % a->period);
        else if (a->type == ANIM_RANDOM)
            a->nexttic = bcnt + 1 + a->data2 + (M_Random() % a->data1);
        else if (a->type == ANIM_LEVEL)
            a->nexttic = bcnt + 1;
    }

}

void WI_updateAnimatedBack(void)
{
    int         i;
    anim_t      *a;

    if (gamemode == commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];

        if (bcnt == a->nexttic)
        {
            switch (a->type)
            {
                case ANIM_ALWAYS:
                    if (++a->ctr >= a->nanims)
                        a->ctr = 0;
                    a->nexttic = bcnt + a->period;
                    break;

                case ANIM_RANDOM:
                    a->ctr++;
                    if (a->ctr == a->nanims)
                    {
                        a->ctr = -1;
                        a->nexttic = bcnt + a->data2 + (M_Random() % a->data1);
                    }
                    else
                        a->nexttic = bcnt + a->period;
                    break;

                case ANIM_LEVEL:
                    // gawd-awful hack for level anims
                    if (!(state == StatCount && i == 7)
                        && wbs->next == a->data1)
                    {
                        a->ctr++;
                        if (a->ctr == a->nanims)
                            a->ctr--;
                        a->nexttic = bcnt + a->period;
                    }
                    break;
            }
        }

    }

}

void WI_drawAnimatedBack(void)
{
    int                 i;
    anim_t              *a;

    if (gamemode == commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];

        if (a->ctr >= 0)
            V_DrawPatch(a->loc.x, a->loc.y, FB, a->p[a->ctr]);
    }

}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//  otherwise only use as many as necessary.
// Returns new x position.
//

int WI_drawNum(int x, int y, int n, int digits)
{

    int         fontwidth = SHORT(num[0]->width);
    int         neg;
    int         temp;

    if (digits < 0)
    {
        if (!n)
        {
            // make variable-length zeros 1 digit long
            digits = 1;
        }
        else
        {
            // figure out # of digits in #
            digits = 0;
            temp = n;

            while (temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }

    neg = n < 0;
    if (neg)
        n = -n;

    // if non-number, do not draw it
    if (n == 1994)
        return 0;

    // draw the new number
    while (digits--)
    {
        x -= fontwidth;
        x += 2 * (n % 10 == 1);
        V_DrawPatchWithShadow(x + 1, y + 1, FB, num[n % 10], true);
        x -= 2 * (n % 10 == 1);
        n /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        V_DrawPatch(x -= 8, y, FB, wiminus);

    return x;
}

void WI_drawPercent(int x, int y, int p)
{
    if (p < 0)
        return;

    V_DrawPatchWithShadow(x + 1, y + 1, FB, percent, false);
    WI_drawNum(x, y, p, -1);
}



//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
void WI_drawTime(int x, int y, int t)
{
    int         div;
    int         n;

    if (t < 0)
        return;

    if (t <= 61 * 59)
    {
        div = 1;

        do
        {
            n = (t / div) % 60;
            x = WI_drawNum(x, y, n, 2) - SHORT(colon->width);
            div *= 60;

            // draw
            if (div == 60 || t / div)
            {
                V_DrawPatchWithShadow(x + 1, y + 1, FB, colon, true);
            }

        }
        while (t / div);
        if (t < 60)
            WI_drawNum(x, y, 0, 2);
    }
    else
    {
        // "sucks"
        V_DrawPatchWithShadow(x + 13 - SHORT(sucks->width), y + 1, FB, sucks, false);
    }
}

void WI_unloadData(void);

void WI_End(void)
{
    WI_unloadData();
}

void WI_initNoState(void)
{
    state = NoState;
    acceleratestage = 0;
    cnt = (gamemode == commercial ? 1 * TICRATE : 10);
}

void WI_updateNoState(void)
{
    WI_updateAnimatedBack();

    if (!--cnt)
    {
        G_WorldDone();
    }

}

static boolean snl_pointeron = false;


void WI_initShowNextLoc(void)
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;

    WI_initAnimatedBack();
}

void WI_updateShowNextLoc(void)
{
    WI_updateAnimatedBack();

    if (!--cnt || acceleratestage)
        WI_initNoState();
    else
        snl_pointeron = ((cnt & 31) < 20);
}

void WI_drawShowNextLoc(void)
{

    int         i;
    int         last;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    if (gamemode != commercial)
    {
        if (wbs->epsd > 2)
        {
            WI_drawEL();
            return;
        }

        last = (wbs->last == 8 ? wbs->next - 1 : wbs->last);

        // draw a splat on taken cities.
        for (i = 0; i <= last; i++)
            WI_drawOnLnode(i, splat);

        // splat the secret level?
        if (wbs->didsecret)
            WI_drawOnLnode(8, splat);

        // draw flashing ptr
        if (snl_pointeron)
            WI_drawOnLnode(wbs->next, yah);
    }

    // draws which level you are entering..
    if (gamemode != commercial
        || (gamemission != pack_nerve && wbs->next != 30)
        || (gamemission == pack_nerve && wbs->next != 8))
        WI_drawEL();

}

void WI_drawNoState(void)
{
    snl_pointeron = true;
    WI_drawShowNextLoc();
}

int WI_fragSum(int playernum)
{
    int         i;
    int         frags = 0;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i]
            && i != playernum)
        {
            frags += plrs[playernum].frags[i];
        }
    }

    frags -= plrs[playernum].frags[playernum];

    return frags;
}



static int      dm_state;
static int      dm_frags[MAXPLAYERS][MAXPLAYERS];
static int      dm_totals[MAXPLAYERS];



void WI_initDeathmatchStats(void)
{

    int         i;
    int         j;

    state = StatCount;
    acceleratestage = 0;
    dm_state = 1;

    cnt_pause = TICRATE;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            for (j = 0; j < MAXPLAYERS; j++)
                if (playeringame[j])
                    dm_frags[i][j] = 0;

            dm_totals[i] = 0;
        }
    }

    WI_initAnimatedBack();
}



void WI_updateDeathmatchStats(void)
{

    int         i;
    int         j;

    boolean     stillticking;

    WI_updateAnimatedBack();

    if (acceleratestage && dm_state != 4)
    {
        acceleratestage = 0;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i])
            {
                for (j = 0; j < MAXPLAYERS; j++)
                    if (playeringame[j])
                        dm_frags[i][j] = plrs[i].frags[j];

                dm_totals[i] = WI_fragSum(i);
            }
        }


        S_StartSound(NULL, sfx_barexp);
        dm_state = 4;
    }


    if (dm_state == 2)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        stillticking = false;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i])
            {
                for (j = 0; j < MAXPLAYERS; j++)
                {
                    if (playeringame[j]
                        && dm_frags[i][j] != plrs[i].frags[j])
                    {
                        if (plrs[i].frags[j] < 0)
                            dm_frags[i][j]--;
                        else
                            dm_frags[i][j]++;

                        if (dm_frags[i][j] > 99)
                            dm_frags[i][j] = 99;

                        if (dm_frags[i][j] < -99)
                            dm_frags[i][j] = -99;

                        stillticking = true;
                    }
                }
                dm_totals[i] = WI_fragSum(i);

                if (dm_totals[i] > 99)
                    dm_totals[i] = 99;

                if (dm_totals[i] < -99)
                    dm_totals[i] = -99;
            }

        }
        if (!stillticking)
        {
            S_StartSound(NULL, sfx_barexp);
            dm_state++;
        }

    }
    else if (dm_state == 4)
    {
        if (acceleratestage)
        {
            S_StartSound(NULL, sfx_slop);

            if (gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if (dm_state & 1)
    {
        if (!--cnt_pause)
        {
            dm_state++;
            cnt_pause = TICRATE;
        }
    }
}



void WI_drawDeathmatchStats(void)
{

    int         i;
    int         j;
    int         x;
    int         y;
    int         w;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();
    WI_drawLF();

    // draw stat titles (top line)
    V_DrawPatch(DM_TOTALSX - SHORT(total->width) / 2,
                DM_MATRIXY - WI_SPACINGY + 10,
                FB,
                total);

    V_DrawPatch(DM_KILLERSX, DM_KILLERSY, FB, killers);
    V_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, FB, victims);

    // draw P?
    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            V_DrawPatch(x - SHORT(p[i]->width) / 2,
                        DM_MATRIXY - WI_SPACINGY,
                        FB,
                        p[i]);

            V_DrawPatch(DM_MATRIXX - SHORT(p[i]->width) / 2,
                        y,
                        FB,
                        p[i]);

            if (i == me)
            {
                V_DrawPatch(x - SHORT(p[i]->width) / 2,
                            DM_MATRIXY - WI_SPACINGY,
                            FB,
                            bstar);

                V_DrawPatch(DM_MATRIXX - SHORT(p[i]->width) / 2,
                            y,
                            FB,
                            star);
            }
        }
        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // draw stats
    y = DM_MATRIXY + 10;
    w = SHORT(num[0]->width);

    for (i = 0; i < MAXPLAYERS; i++)
    {
        x = DM_MATRIXX + DM_SPACINGX;

        if (playeringame[i])
        {
            for (j = 0; j < MAXPLAYERS; j++)
            {
                if (playeringame[j])
                    WI_drawNum(x + w, y, dm_frags[i][j], 2);

                x += DM_SPACINGX;
            }
            WI_drawNum(DM_TOTALSX + w, y, dm_totals[i], 2);
        }
        y += WI_SPACINGY;
    }
}

static int      cnt_frags[MAXPLAYERS];
static int      dofrags;
static int      ng_state;

void WI_initNetgameStats(void)
{
    int i;

    state = StatCount;
    acceleratestage = 0;
    ng_state = 1;

    cnt_pause = TICRATE;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
            continue;

        cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

        dofrags += WI_fragSum(i);
    }

    dofrags = !!dofrags;

    WI_initAnimatedBack();
}



void WI_updateNetgameStats(void)
{

    int         i;
    int         fsum;

    boolean     stillticking;

    WI_updateAnimatedBack();

    if (acceleratestage && ng_state != 10)
    {
        acceleratestage = 0;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (!playeringame[i])
                continue;

            cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
            cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
            cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;

            if (dofrags)
                cnt_frags[i] = WI_fragSum(i);
        }
        S_StartSound(NULL, sfx_barexp);
        ng_state = 10;
    }

    if (ng_state == 2)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        stillticking = false;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (!playeringame[i])
                continue;

            cnt_kills[i] += 2;

            if (cnt_kills[i] >= (plrs[i].skills * 100) / wbs->maxkills)
                cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            S_StartSound(NULL, sfx_barexp);
            ng_state++;
        }
    }
    else if (ng_state == 4)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        stillticking = false;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (!playeringame[i])
                continue;

            cnt_items[i] += 2;
            if (cnt_items[i] >= (plrs[i].sitems * 100) / wbs->maxitems)
                cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
            else
                stillticking = true;
        }
        if (!stillticking)
        {
            S_StartSound(NULL, sfx_barexp);
            ng_state++;
        }
    }
    else if (ng_state == 6)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        stillticking = false;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (!playeringame[i])
                continue;

            cnt_secret[i] += 2;

            if (cnt_secret[i] >= (plrs[i].ssecret * 100) / wbs->maxsecret)
                cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            S_StartSound(NULL, sfx_barexp);
            ng_state += 1 + 2 * !dofrags;
        }
    }
    else if (ng_state == 8)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        stillticking = false;

        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (!playeringame[i])
                continue;

            cnt_frags[i] += 1;

            if (cnt_frags[i] >= (fsum = WI_fragSum(i)))
                cnt_frags[i] = fsum;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            S_StartSound(NULL, sfx_pldeth);
            ng_state++;
        }
    }
    else if (ng_state == 10)
    {
        if (acceleratestage)
        {
            S_StartSound(NULL, sfx_sgcock);
            if (gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if (ng_state & 1)
    {
        if (!--cnt_pause)
        {
            ng_state++;
            cnt_pause = TICRATE;
        }
    }
}



void WI_drawNetgameStats(void)
{
    int         i;
    int         x;
    int         y;
    int         pwidth = SHORT(percent->width);

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    WI_drawLF();

    // draw stat titles (top line)
    V_DrawPatch(NG_STATSX + NG_SPACINGX - SHORT(kills->width),
                NG_STATSY, FB, kills);

    V_DrawPatch(NG_STATSX + 2 * NG_SPACINGX - SHORT(items->width),
                NG_STATSY, FB, items);

    V_DrawPatch(NG_STATSX + 3 * NG_SPACINGX - SHORT(secret->width),
                NG_STATSY, FB, secret);

    if (dofrags)
        V_DrawPatch(NG_STATSX + 4 * NG_SPACINGX - SHORT(frags->width),
                    NG_STATSY, FB, frags);

    // draw stats
    y = NG_STATSY + SHORT(kills->height);

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
            continue;

        x = NG_STATSX;
        V_DrawPatch(x - SHORT(p[i]->width), y, FB, p[i]);

        if (i == me)
            V_DrawPatch(x - SHORT(p[i]->width), y, FB, star);

        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_kills[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_items[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_secret[i]);
        x += NG_SPACINGX;

        if (dofrags)
            WI_drawNum(x, y + 10, cnt_frags[i], -1);

        y += WI_SPACINGY;
    }

}

static int      sp_state;

void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;

    WI_initAnimatedBack();
}

void WI_updateStats(void)
{

    WI_updateAnimatedBack();

    if (acceleratestage && sp_state != 10)
    {
        acceleratestage = 0;
        cnt_kills[0] = (int)(plrs[me].skills * 100) / wbs->maxkills;
        if (cnt_kills[0] > 100)
            cnt_kills[0] = 100;
        cnt_items[0] = (int)(plrs[me].sitems * 100) / wbs->maxitems;
        if (cnt_items[0] > 100)
            cnt_items[0] = 100;
        cnt_secret[0] = (int)(plrs[me].ssecret * 100) / wbs->maxsecret;
        if (cnt_secret[0] > 100)
            cnt_secret[0] = 100;
        cnt_time = (int)(plrs[me].stime) / TICRATE;
        cnt_par = (int)(wbs->partime) / TICRATE;
        S_StartSound(NULL, sfx_barexp);
        sp_state = 10;
    }

    if (sp_state == 2)
    {
        cnt_kills[0] += 2;

        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        if (cnt_kills[0] >= (int)(plrs[me].skills * 100) / wbs->maxkills)
        {
            cnt_kills[0] = (int)(plrs[me].skills * 100) / wbs->maxkills;
            S_StartSound(NULL, sfx_barexp);
            sp_state++;
        }
    }
    else if (sp_state == 4)
    {
        cnt_items[0] += 2;

        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        if (cnt_items[0] >= (int)(plrs[me].sitems * 100) / wbs->maxitems)
        {
            cnt_items[0] = (int)(plrs[me].sitems * 100) / wbs->maxitems;
            S_StartSound(NULL, sfx_barexp);
            sp_state++;
        }
    }
    else if (sp_state == 6)
    {
        cnt_secret[0] += 2;

        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        if (cnt_secret[0] >= (int)(plrs[me].ssecret * 100) / wbs->maxsecret)
        {
            cnt_secret[0] = (int)(plrs[me].ssecret * 100) / wbs->maxsecret;
            S_StartSound(NULL, sfx_barexp);
            sp_state++;
        }
    }

    else if (sp_state == 8)
    {
        if (!(bcnt & 3))
            S_StartSound(NULL, sfx_pistol);

        cnt_time += 3;

        if (cnt_time >= (int)(plrs[me].stime) / TICRATE)
            cnt_time = (int)(plrs[me].stime) / TICRATE;

        cnt_par += 3;

        if (cnt_par >= (int)(wbs->partime) / TICRATE)
        {
            cnt_par = (int)(wbs->partime) / TICRATE;

            if (cnt_time >= (int)(plrs[me].stime) / TICRATE)
            {
                S_StartSound(NULL, sfx_barexp);
                sp_state++;
            }
        }
    }
    else if (sp_state == 10)
    {
        if (acceleratestage)
        {
            S_StartSound(NULL, sfx_sgcock);

            if (gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if (sp_state & 1)
    {
        if (!--cnt_pause)
        {
            sp_state++;
            cnt_pause = TICRATE;
        }
    }

}

extern void M_DrawString(int x, int y, char *str);
extern boolean canmodify;

void WI_drawStats(void)
{
    // line height
    int lh;

    lh = (3 * SHORT(num[0]->height)) / 2;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    WI_drawLF();

    V_DrawPatchWithShadow(SP_STATSX + 1, SP_STATSY + 1, FB, kills, false);
    WI_drawPercent(ORIGINALWIDTH - SP_STATSX - 14, SP_STATSY, cnt_kills[0]);

    V_DrawPatchWithShadow(SP_STATSX + 1, SP_STATSY + lh + 1, FB, items, false);
    WI_drawPercent(ORIGINALWIDTH - SP_STATSX - 14, SP_STATSY + lh, cnt_items[0]);

    if (totalsecret)
    {
        if (!WISCRT2)
        {
            M_DrawString(SP_STATSX, SP_STATSY + 2 * lh - 3, "secrets");
        }
        else
        {
            V_DrawPatchWithShadow(SP_STATSX + 1, SP_STATSY + 2 * lh + 1, FB, sp_secret, false);
        }

        WI_drawPercent(ORIGINALWIDTH - SP_STATSX - 14, SP_STATSY + 2 * lh, cnt_secret[0]);
    }

    V_DrawPatchWithShadow(SP_TIMEX + 1, SP_TIMEY + 1, FB, timepatch, false);
    WI_drawTime(ORIGINALWIDTH / 2 - SP_TIMEX * 2, SP_TIMEY, cnt_time);

    if (canmodify)
    {
        V_DrawPatchWithShadow(ORIGINALWIDTH/2 + SP_TIMEX * 2 + 5, SP_TIMEY + 1, FB, par, false);
        WI_drawTime(ORIGINALWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
    }
}

void WI_checkForAccelerate(void)
{
    int         i;
    player_t    *player;

    // check for button presses to skip delays
    for (i = 0, player = players; i < MAXPLAYERS; i++, player++)
    {
        if (playeringame[i] && !menuactive)
        {
            Uint8 *keystate = SDL_GetKeyState(NULL);

            if ((player->cmd.buttons & BT_ATTACK)
                || keystate[SDLK_RETURN] || keystate[SDLK_KP_ENTER])
            {
                if (!player->attackdown)
                    acceleratestage = 1;
                player->attackdown = true;
            }
            else
            {
                player->attackdown = false;
            }
            if (player->cmd.buttons & BT_USE)
            {
                if (!player->usedown)
                    acceleratestage = 1;
                player->usedown = true;
            }
            else
            {
                player->usedown = false;
            }
        }
    }
}



// Updates stuff each tick
void WI_Ticker(void)
{

    if (menuactive || paused)
      return;

    // counter for general background animation
    bcnt++;

    if (bcnt == 1)
    {
        // intermission music
        if (gamemode == commercial)
            S_ChangeMusic(mus_dm2int, true, false);
        else
            S_ChangeMusic(mus_inter, true, false);
    }

    WI_checkForAccelerate();

    switch (state)
    {
        case StatCount:
            if (deathmatch)
                WI_updateDeathmatchStats();
            else if (netgame)
                WI_updateNetgameStats();
            else
                WI_updateStats();
            break;

        case ShowNextLoc:
            WI_updateShowNextLoc();
            break;

        case NoState:
            WI_updateNoState();
            break;
    }
}

typedef void (*load_callback_t)(char *lumpname, patch_t **variable);

// Common load/unload function.  Iterates over all the graphics
// lumps to be loaded/unloaded into memory.

static void WI_loadUnloadData(load_callback_t callback)
{
    int         i;
    int         j;
    char        name[9];
    anim_t      *a;

    if (gamemode == commercial)
    {
        for (i = 0; i < NUMCMAPS; i++)
        {
            snprintf(name, 9, "CWILV%2.2d", i);
            callback(name, &lnames[i]);
        }
    }
    else
    {
        for (i = 0; i < NUMMAPS; i++)
        {
            snprintf(name, 9, "WILV%d%d", wbs->epsd, i);
            callback(name, &lnames[i]);
        }

        // you are here
        callback("WIURH0", &yah[0]);

        // you are here (alt.)
        callback("WIURH1", &yah[1]);

        // splat
        callback("WISPLAT", &splat[0]);

        if (wbs->epsd < 3)
        {
            for (j = 0; j < NUMANIMS[wbs->epsd]; j++)
            {
                a = &anims[wbs->epsd][j];
                for (i = 0; i < a->nanims; i++)
                {
                    // MONDO HACK!
                    if (wbs->epsd != 1 || j != 8)
                    {
                        // animations
                        snprintf(name, 9, "WIA%d%.2d%.2d", wbs->epsd, j, i);
                        callback(name, &a->p[i]);
                    }
                    else
                    {
                        // HACK ALERT!
                        a->p[i] = anims[1][4].p[i];
                    }
                }
            }
        }
    }

    // More hacks on minus sign.
    callback("WIMINUS", &wiminus);

    for (i = 0; i < 10; i++)
    {
        // numbers 0-9
        snprintf(name, 9, "WINUM%d", i);
        callback(name, &num[i]);
    }

    // percent sign
    callback("WIPCNT", &percent);

    // "finished"
    callback("WIF", &finished);

    // "entering"
    callback("WIENTER", &entering);

    // "kills"
    callback("WIOSTK", &kills);

    // "scrt"
    callback("WIOSTS", &secret);

    // "secret"
    callback("WISCRT2", &sp_secret);

    // french wad uses WIOBJ (?)
    if (W_CheckNumForName("WIOBJ") >= 0)
    {
        // "items"
        if (netgame && !deathmatch)
            callback("WIOBJ", &items);
        else
            callback("WIOSTI", &items);
    }
    else
    {
        callback("WIOSTI", &items);
    }

    // "frgs"
    callback("WIFRGS", &frags);

    // ":"
    callback("WICOLON", &colon);

    // "time"
    callback("WITIME", &timepatch);

    // "sucks"
    callback("WISUCKS", &sucks);

    // "par"
    callback("WIPAR", &par);

    // "killers" (vertical)
    callback("WIKILRS", &killers);

    // "victims" (horiz)
    callback("WIVCTMS", &victims);

    // "total"
    callback("WIMSTT", &total);

    for (i = 0; i < MAXPLAYERS; i++)
    {
        // "1,2,3,4"
        snprintf(name, 9, "STPB%d", i);
        callback(name, &p[i]);

        // "1,2,3,4"
        snprintf(name, 9, "WIBP%d", i + 1);
        callback(name, &bp[i]);
    }
}

static void WI_loadCallback(char *name, patch_t **variable)
{
    *variable = (patch_t *)W_CacheLumpName(name, PU_STATIC);
}

void WI_loadData(void)
{
    char        bg_lumpname[9];
    patch_t     *bg;

    if (gamemode == commercial)
    {
        NUMCMAPS = 32 + (W_CheckNumForName("MAP33") >= 0);
        lnames = (patch_t **)Z_Malloc(sizeof(patch_t *) * NUMCMAPS, PU_STATIC, NULL);
    }
    else
    {
        lnames = (patch_t **)Z_Malloc(sizeof(patch_t *) * NUMMAPS, PU_STATIC, NULL);
    }

    WI_loadUnloadData(WI_loadCallback);

    // These two graphics are special cased because we're sharing
    // them with the status bar code

    // your face
    star = (patch_t *)W_CacheLumpName("STFST01", PU_STATIC);

    // dead face
    bstar = (patch_t *)W_CacheLumpName("STFDEAD0", PU_STATIC);

    // Background image

    if (gamemode == commercial
        || (gamemode == retail && wbs->epsd == 3))
    {
        strncpy(bg_lumpname, "INTERPIC", 9);
        bg_lumpname[8] = '\0';
    }
    else
    {
        snprintf(bg_lumpname, 9, "WIMAP%d", wbs->epsd);
    }
    bg = (patch_t *)W_CacheLumpName(bg_lumpname, PU_CACHE);
    V_DrawPatch(0, 0, 1, bg);
}

static void WI_unloadCallback(char *name, patch_t **variable)
{
    W_ReleaseLumpName(name);
    *variable = NULL;
}

void WI_unloadData(void)
{
    WI_loadUnloadData(WI_unloadCallback);
}

void WI_Drawer(void)
{
    switch (state)
    {
        case StatCount:
            if (deathmatch)
                WI_drawDeathmatchStats();
            else if (netgame)
                WI_drawNetgameStats();
            else
                WI_drawStats();
            break;

        case ShowNextLoc:
            WI_drawShowNextLoc();
            break;

        case NoState:
            WI_drawNoState();
            break;
    }
}

extern void P_MapName(int episode, int map);
extern char maptitle[128];

void WI_initVariables(wbstartstruct_t *wbstartstruct)
{
    wbs = wbstartstruct;

    acceleratestage = 0;
    cnt = bcnt = 0;
    firstrefresh = 1;
    me = wbs->pnum;
    plrs = wbs->plyr;

    if (!wbs->maxkills)
        wbs->maxkills = 1;

    if (!wbs->maxitems)
        wbs->maxitems = 1;

    if (!wbs->maxsecret)
        wbs->maxsecret = 1;

    if (gamemode != retail && wbs->epsd > 2)
        wbs->epsd -= 3;

    mapname = (char *)Z_Malloc(128, PU_STATIC, NULL);
    strcpy(mapname, maptitle);
    nextmapname = (char *)Z_Malloc(128, PU_STATIC, NULL);
    P_MapName(wbs->epsd + 1, wbs->next + 1);
    strcpy(nextmapname, maptitle);
}

void WI_Start(wbstartstruct_t *wbstartstruct)
{
    WI_initVariables(wbstartstruct);
    WI_loadData();

    if (deathmatch)
        WI_initDeathmatchStats();
    else if (netgame)
        WI_initNetgameStats();
    else
        WI_initStats();
}