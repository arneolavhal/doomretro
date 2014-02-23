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

#include "SDL.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"

extern int     vibrationtics;
extern int     followplayer;
extern boolean oldweaponsowned[];

void G_RemoveChoppers(void);

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB 0x100000

boolean onground;

//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_Thrust(player_t *player, angle_t angle, fixed_t move)
{
    angle >>= ANGLETOFINESHIFT;

    player->mo->momx += FixedMul(move, finecosine[angle]);
    player->mo->momy += FixedMul(move, finesine[angle]);
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight(player_t *player)
{
    if (!onground)
    {
        player->viewz = player->mo->z + VIEWHEIGHT;

        if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
            player->viewz = player->mo->ceilingz - 4 * FRACUNIT;
        return;
    }

    if (player->playerstate == PST_LIVE)
    {
        int     angle;
        fixed_t bob;

        // Regular movement bobbing
        // (needs to be calculated for gun swing
        // even if not on ground)
        // OPTIMIZE: tablify angle
        // Note: a LUT allows for effects
        //  like a ramp with low health.
        player->bob = (FixedMul(player->mo->momx, player->mo->momx)
                      + FixedMul(player->mo->momy, player->mo->momy)) >> 2;

        if (player->bob > MAXBOB)
            player->bob = MAXBOB;

        angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
        bob = FixedMul(player->bob / 2, finesine[angle]);

        // move viewheight
        player->viewheight += player->deltaviewheight;

        if (player->viewheight > VIEWHEIGHT)
        {
            player->viewheight = VIEWHEIGHT;
            player->deltaviewheight = 0;
        }

        if (player->viewheight < VIEWHEIGHT / 2)
        {
            player->viewheight = VIEWHEIGHT / 2;
            if (player->deltaviewheight <= 0)
                player->deltaviewheight = 1;
        }

        if (player->deltaviewheight)
        {
            player->deltaviewheight += FRACUNIT / 4;
            if (!player->deltaviewheight)
                player->deltaviewheight = 1;
        }
        player->viewz = player->mo->z + player->viewheight + bob;

    }
    else
        player->viewz = player->mo->z + player->viewheight;

    if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
        player->viewz = player->mo->ceilingz - 4 * FRACUNIT;
}

//
// P_MovePlayer
//
void P_MovePlayer(player_t *player)
{
    ticcmd_t *cmd = &player->cmd;
    mobj_t   *mo = player->mo;

    mo->angle += (cmd->angleturn << 16);

    // Do not let the player control movement
    //  if not onground.
    onground = (mo->z <= mo->floorz);

    if (onground)
    {
        if (cmd->forwardmove)
            P_Thrust(player, mo->angle, cmd->forwardmove * 2048);

        if (cmd->sidemove)
            P_Thrust(player, mo->angle - ANG90, cmd->sidemove * 2048);
    }

    if ((cmd->forwardmove || cmd->sidemove) && mo->state == &states[S_PLAY])
        P_SetMobjState(mo, S_PLAY_RUN1);
}

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5 (ANG90 / 18)

void P_DeathThink(player_t *player)
{
    angle_t        angle;
    angle_t        delta;
//#error SDL_GetKeyState() has been renamed to SDL_GetKeyboardState(). The returned array should now be indexed by SDL_SCANCODE_* values (see SDL_Scancode) instead of SDL_Keysym values. https://wiki.libsdl.org/MigrationGuide
//    Uint8          *keystate = SDL_GetKeyState(NULL);
	// ao: WI_checkForAccelerate working?
	const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    static int     count = 0;
    static boolean facingkiller = false;

    P_MovePsprites(player);

    // fall to the ground
    player->deltaviewheight = 0;
    onground = (player->mo->z <= player->mo->floorz);
    if (onground)
    {
        if (player->viewheight > 6 * FRACUNIT)
            player->viewheight -= FRACUNIT;

        if (player->viewheight < 6 * FRACUNIT)
            player->viewheight = 6 * FRACUNIT;
    }
    P_CalcHeight(player);

    if (player->attacker && player->attacker != player->mo && !facingkiller)
    {
        angle = R_PointToAngle2(player->mo->x,
                                player->mo->y,
                                player->attacker->x,
                                player->attacker->y);

        delta = angle - player->mo->angle;

        if (delta < ANG5 || delta > (unsigned)-ANG5)
        {
            // Looking at killer,
            //  so fade damage flash down.
            player->mo->angle = angle;

            if (player->damagecount > 0)
                player->damagecount--;

            facingkiller = true;
        }
        else if (delta < ANG180)
            player->mo->angle += ANG5;
        else
            player->mo->angle -= ANG5;
    }
    else if (player->damagecount > 0)
        player->damagecount--;

    if (((player->cmd.buttons & BT_USE) 
        || ((player->cmd.buttons & BT_ATTACK) && !player->damagecount && count > TICRATE * 2)
        || keystate[SDL_SCANCODE_RETURN]))
    {
        count = 0;
        vibrationtics = 1;
        player->playerstate = PST_REBORN;
        facingkiller = false;
    }
    else
        count++;
}

//
// P_PlayerThink
//
void P_PlayerThink(player_t *player)
{
    ticcmd_t     *cmd = &player->cmd;
    weapontype_t newweapon;

    if (player->cheats & CF_NOCLIP)
        player->mo->flags |= MF_NOCLIP;
    else
        player->mo->flags &= ~MF_NOCLIP;

    // chainsaw run forward
    if (player->mo->flags & MF_JUSTATTACKED)
    {
        cmd->angleturn = 0;
        cmd->forwardmove = 0xc800 / 512;
        cmd->sidemove = 0;
        player->mo->flags &= ~MF_JUSTATTACKED;
    }

    if (player->playerstate == PST_DEAD)
    {
        P_DeathThink(player);
        return;
    }

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (player->mo->reactiontime)
        player->mo->reactiontime--;
    else
        P_MovePlayer(player);

    P_CalcHeight(player);

    if (player->mo->subsector->sector->special)
        P_PlayerInSpecialSector(player);

    // Check for weapon change.

    // A special event has no other buttons.
    if (cmd->buttons & BT_SPECIAL)
        cmd->buttons = 0;

    if ((cmd->buttons & BT_CHANGE) && (!automapactive || (automapactive && followplayer)))
    {
        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        newweapon = (weapontype_t)((cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT);

        //if (newweapon == wp_chainsaw)
        //    newweapon = wp_nochange;
        //else if (newweapon == wp_fist
        //         && player->weaponowned[wp_chainsaw]
        //         && !(player->readyweapon == wp_chainsaw
        //              && player->powers[pw_strength]))
        //{
        //    newweapon = wp_chainsaw;
        //}
        if (newweapon == wp_chainsaw)
            newweapon = wp_nochange;
        else if (newweapon == wp_fist)
        {
            if (player->readyweapon == wp_fist)
            {
                if (player->weaponowned[wp_chainsaw])
                {
                    player->fistorchainsaw = wp_chainsaw;
                    newweapon = wp_chainsaw;
                }
            }
            else if (player->readyweapon == wp_chainsaw)
            {
                if (player->powers[pw_strength])
                {
                    player->fistorchainsaw = wp_fist;
                    newweapon = wp_fist;
                }
                else
                {
                    newweapon = wp_nochange;
                }
            }
            else
            {
                newweapon = player->fistorchainsaw;
            }
        }

        // Don't switch to a weapon without any or enough ammo.
        else if (((newweapon == wp_pistol
                   || newweapon == wp_chaingun)
                  && !player->ammo[am_clip])
                 || (newweapon == wp_shotgun
                     && !player->ammo[am_shell])
                 || (newweapon == wp_missile
                     && !player->ammo[am_misl])
                 || (newweapon == wp_plasma
                     && !player->ammo[am_cell])
                 || (newweapon == wp_bfg
                     && player->ammo[am_cell] < 40))
        {
            newweapon = wp_nochange;
        }

        // Select the preferred shotgun.
        if (newweapon == wp_shotgun)
        {
            if ((!player->weaponowned[wp_shotgun]
                || player->readyweapon == wp_shotgun)
                && player->weaponowned[wp_supershotgun]
                && player->ammo[am_shell] >= 2)
                player->preferredshotgun = wp_supershotgun;
            else if (player->readyweapon == wp_supershotgun
                     || (player->preferredshotgun == wp_supershotgun
                         && player->ammo[am_shell] == 1))
                player->preferredshotgun = wp_shotgun;
            newweapon = player->preferredshotgun;
        }

        if (player->weaponowned[newweapon] && newweapon != player->readyweapon)
        {
            // Do not go to plasma or BFG in shareware,
            //  even if cheated.
            // [BH] but weaponowned[] isn't going to be set, so no need to check
            //if ((newweapon != wp_plasma
            //     && newweapon != wp_bfg)
            //    || gamemode != shareware)
            //{
                player->pendingweapon = newweapon;
            //}
        }

        if ((player->cheats & CF_CHOPPERS) && newweapon != wp_chainsaw)
            G_RemoveChoppers();
    }

    // check for use
    if (cmd->buttons & BT_USE)
    {
        if (!player->usedown)
        {
            P_UseLines(player);
            player->usedown = true;
        }
    }
    else
        player->usedown = false;

    // cycle psprites
    P_MovePsprites(player);

    // Counters, time dependent power ups.

    // Strength counts up to diminish fade.
    if (player->powers[pw_strength])
        player->powers[pw_strength]++;

    if (player->powers[pw_invulnerability] > 0)
        player->powers[pw_invulnerability]--;

    if (player->powers[pw_invisibility] > 0)
        if (!(--player->powers[pw_invisibility]))
            player->mo->flags &= ~MF_SHADOW;

    if (player->powers[pw_infrared] > 0)
        player->powers[pw_infrared]--;

    if (player->powers[pw_ironfeet] > 0)
        player->powers[pw_ironfeet]--;

    if (player->damagecount)
        player->damagecount--;

    if (player->bonuscount)
        player->bonuscount--;

    // Handling colormaps.
    if (player->powers[pw_invulnerability] > 4 * 32
        || (player->powers[pw_invulnerability] & 8))
        player->fixedcolormap = INVERSECOLORMAP;
    else player->fixedcolormap = (player->powers[pw_infrared] > 4 * 32
                                  || (player->powers[pw_infrared] & 8));
}
