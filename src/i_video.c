/*
====================================================================

DOOM RETRO
A classic, refined DOOM source port. For Windows PC.

Copyright © 1993-1996 id Software LLC, a ZeniMax Media company.
Copyright © 2005-2014 Simon Howard.
Copyright © 2013-2014 Brad Harding.

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

#include <math.h>
#include "d_main.h"
#include "doomstat.h"
#include "hu_stuff.h"
#include "i_gamepad.h"
#include "i_system.h"
#include "i_tinttab.h"
#include "i_video.h"
#include "m_config.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include "s_sound.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// Window position:

char *windowposition = "";

static SDL_Surface *screen = NULL;
SDL_Window *sdl_window = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture *sdl_texture = NULL;

// Intermediate 8-bit buffer that we draw to instead of 'screen'.
// This is used when we are rendering in 32-bit screen mode.
// When in a real 8-bit screen mode, screenbuffer == screen.

static SDL_Surface *screenbuffer = NULL;

// palette

static SDL_Color palette[256];
static boolean   palette_to_set;

// display has been set up?

static boolean initialized = false;

// Bit mask of mouse button state.

static unsigned int mouse_button_state = 0;

// Fullscreen width and height

int screenwidth = 0;
int screenheight = 0;

// Window width and height

int windowwidth = SCREENWIDTH;
int windowheight = SCREENWIDTH * 3 / 4;

// Run in full screen mode?

boolean fullscreen = true;

boolean widescreen = false;
boolean returntowidescreen = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

boolean window_focused;

// Empty mouse cursor

static SDL_Cursor *emptycursor;

// Window resize state.

boolean      need_resize = false;
unsigned int resize_h;

int desktopwidth;
int desktopheight;

char *videodriver = "windib";
char envstring[255];

static int width;
static int height;
static int stepx;
static int stepy;
static int startx;
static int starty;
static int pitch;

byte *pixels;

boolean keys[UCHAR_MAX];

byte gammatable[GAMMALEVELS][256];

double gammalevel[GAMMALEVELS] =
{
    0.5, 0.625, 0.75, 0.875, 1.0, 1.25, 1.5, 1.75, 2.0
};

// Gamma correction level to use
int usegamma = USEGAMMA_DEFAULT;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = MOUSEACCELERATION_DEFAULT;
int   mouse_threshold = MOUSETHRESHOLD_DEFAULT;

static void ApplyWindowResize(int height);
static void SetWindowPositionVars(void);

boolean MouseShouldBeGrabbed()
{
    // if the window doesn't have focus, never grab it

    if (!window_focused)
        return false;

    // always grab the mouse when full screen (dont want to
    // see the mouse pointer)

    if (fullscreen)
        return true;

    // when menu is active or game is paused, release the mouse

    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL && !demoplayback && !advancedemo);

}

// Update the value of window_focused when we get a focus event
//
// We try to make ourselves be well-behaved: the grab on the mouse
// is removed if we lose focus (such as a popup window appearing),
// and we dont move the mouse around if we aren't focused either.

static void UpdateFocus(void)
{
//#error SDL_GetAppState() -> SDL_GetWindowFlags()
	Uint32 state = SDL_GetWindowFlags(sdl_window);
	//Uint8          state = SDL_GetAppState();
    static boolean alreadypaused = false;

    // Should the screen be grabbed?

    screenvisible = (state & SDL_WINDOW_SHOWN); // changed from SDL_APPACTIVE);

    // We should have input (keyboard) focus and be visible
    // (not minimized)

    window_focused = ((state & SDL_WINDOW_INPUT_FOCUS) && screenvisible);

    if (!window_focused && !menuactive && gamestate == GS_LEVEL && !demoplayback && !advancedemo)
    {
        if (paused)
            alreadypaused = true;
        else
        {
            alreadypaused = false;
            //sendpause = true; // AO: don't pause when losing focus
        }
    }
}

// Show or hide the mouse cursor. We have to use different techniques
// depending on the OS.

static void SetShowCursor(boolean show)
{
    // On Windows, using SDL_ShowCursor() adds lag to the mouse input,
    // so work around this by setting an invisible cursor instead. On
    // other systems, it isn't possible to change the cursor, so this
    // hack has to be Windows-only. (Thanks to entryway for this)

    SDL_SetCursor(emptycursor);

    // When the cursor is hidden, grab the input.

	SDL_SetRelativeMouseMode(!show);
    //SDL_WM_GrabInput((SDL_GrabMode)!show);
}

//
// Translates the SDL key
//

static int TranslateKey(SDL_Keysym *sym)
{
	static char cmdMsg[80];

    switch (sym->scancode)
    {
	case SDL_SCANCODE_A:
			if ( sym->mod & KMOD_SHIFT )
			{
				sprintf(cmdMsg, "CMD: SHIFT A");
			}
			else
			{
				sprintf(cmdMsg, "CMD: A");
			}
			players[consoleplayer].message = cmdMsg;
			return KEY_LEFTARROW;
        case SDL_SCANCODE_D:
			if ( sym->mod & KMOD_SHIFT )
			{
				sprintf(cmdMsg, "CMD: SHIFT D");
			}
			else
			{
				sprintf(cmdMsg, "CMD: D");
			}
			players[consoleplayer].message = cmdMsg;
			return KEY_RIGHTARROW;
        case SDL_SCANCODE_S:
			if ( sym->mod & KMOD_SHIFT )
			{
				sprintf(cmdMsg, "CMD: SHIFT S");
			}
			else
			{
				sprintf(cmdMsg, "CMD: S");
			}
			players[consoleplayer].message = cmdMsg;
			return KEY_DOWNARROW;
        case SDL_SCANCODE_W:
			if ( sym->mod & KMOD_SHIFT )
			{
				sprintf(cmdMsg, "CMD: SHIFT W");
			}
			else
			{
				sprintf(cmdMsg, "CMD: W");
			}
			players[consoleplayer].message = cmdMsg;
			return KEY_UPARROW;
        case SDL_SCANCODE_LEFT:
			sprintf(cmdMsg, "CMD: A");
			players[consoleplayer].message = cmdMsg;
			return KEY_LEFTARROW;
        case SDL_SCANCODE_RIGHT:
			sprintf(cmdMsg, "CMD: D");
			players[consoleplayer].message = cmdMsg;
			return KEY_RIGHTARROW;
        case SDL_SCANCODE_DOWN:
			sprintf(cmdMsg, "CMD: S");
			players[consoleplayer].message = cmdMsg;
			return KEY_DOWNARROW;
        case SDL_SCANCODE_UP:
			sprintf(cmdMsg, "CMD: W");
			players[consoleplayer].message = cmdMsg;
			return KEY_UPARROW;
        case SDL_SCANCODE_ESCAPE:      return KEY_ESCAPE;
        case SDL_SCANCODE_RETURN:
			sprintf(cmdMsg, "CMD: ENTER");
			players[consoleplayer].message = cmdMsg;
			return KEY_ENTER;
        case SDL_SCANCODE_TAB:         return KEY_TAB;
        case SDL_SCANCODE_F1:          return KEY_F1;
        case SDL_SCANCODE_F2:          return KEY_F2;
        case SDL_SCANCODE_F3:          return KEY_F3;
        case SDL_SCANCODE_F4:          return KEY_F4;
        case SDL_SCANCODE_F5:          return KEY_F5;
        case SDL_SCANCODE_F6:          return KEY_F6;
        case SDL_SCANCODE_F7:          return KEY_F7;
        case SDL_SCANCODE_F8:          return KEY_F8;
        case SDL_SCANCODE_F9:          return KEY_F9;
        case SDL_SCANCODE_F10:         return KEY_F10;
        case SDL_SCANCODE_F11:         return KEY_F11;
        case SDL_SCANCODE_F12:         return KEY_F12;
        case SDL_SCANCODE_BACKSPACE:   return KEY_BACKSPACE;
        case SDL_SCANCODE_DELETE:      return KEY_DEL;
        case SDL_SCANCODE_PAUSE:       return KEY_PAUSE;
        case SDL_SCANCODE_EQUALS:      return KEY_EQUALS;
        case SDL_SCANCODE_MINUS:       return KEY_MINUS;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:      return KEY_RSHIFT;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
			sprintf(cmdMsg, "CMD: FIRE");
			players[consoleplayer].message = cmdMsg;
			return KEY_RCTRL;
        case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:			return KEY_RALT;
        case SDL_SCANCODE_LGUI:
        case SDL_SCANCODE_RGUI:			return 0;
        case SDL_SCANCODE_CAPSLOCK:		return KEY_CAPSLOCK;
		case SDL_SCANCODE_SCROLLLOCK:   return KEY_SCRLCK;
        case SDL_SCANCODE_KP_0:         return KEYP_0;
        case SDL_SCANCODE_KP_1:         return KEYP_1;
        case SDL_SCANCODE_KP_2:         return KEYP_2;
        case SDL_SCANCODE_KP_3:         return KEYP_3;
        case SDL_SCANCODE_KP_4:         return KEYP_4;
        case SDL_SCANCODE_KP_5:         return KEYP_5;
        case SDL_SCANCODE_KP_6:         return KEYP_6;
        case SDL_SCANCODE_KP_7:         return KEYP_7;
        case SDL_SCANCODE_KP_8:         return KEYP_8;
        case SDL_SCANCODE_KP_9:         return KEYP_9;
        case SDL_SCANCODE_KP_PERIOD:   return KEYP_PERIOD;
        case SDL_SCANCODE_KP_MULTIPLY: return KEYP_MULTIPLY;
        case SDL_SCANCODE_KP_PLUS:     return KEYP_PLUS;
        case SDL_SCANCODE_KP_MINUS:    return KEYP_MINUS;
        case SDL_SCANCODE_KP_DIVIDE:   return KEYP_DIVIDE;
        case SDL_SCANCODE_KP_EQUALS:   return KEYP_EQUALS;
        case SDL_SCANCODE_KP_ENTER:    return KEYP_ENTER;
        case SDL_SCANCODE_HOME:        return KEY_HOME;
        case SDL_SCANCODE_INSERT:      return KEY_INS;
        case SDL_SCANCODE_END:         return KEY_END;
        case SDL_SCANCODE_PAGEUP:      return KEY_PGUP;
        case SDL_SCANCODE_PAGEDOWN:    return KEY_PGDN;
        case SDL_SCANCODE_PRINTSCREEN: return KEY_PRINT;
        case SDL_SCANCODE_NUMLOCKCLEAR:return KEY_NUMLOCK;
		case SDL_SCANCODE_SPACE:
			sprintf(cmdMsg, "CMD: SPACE");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_1:
			sprintf(cmdMsg, "CMD: 1");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_2:
			sprintf(cmdMsg, "CMD: 2");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_3:
			sprintf(cmdMsg, "CMD: 3");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_4:
			sprintf(cmdMsg, "CMD: 4");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_5:
			sprintf(cmdMsg, "CMD: 5");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_6:
			sprintf(cmdMsg, "CMD: 6");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		case SDL_SCANCODE_7:
			sprintf(cmdMsg, "CMD: 7");
			players[consoleplayer].message = cmdMsg;
			return tolower(sym->sym);
		default:               return tolower(sym->sym);
    }
}

int TranslateKey2(int key)
{
    switch (key)
    {
        case KEY_LEFTARROW:    return SDL_SCANCODE_LEFT;
        case KEY_RIGHTARROW:   return SDL_SCANCODE_RIGHT;
        case KEY_DOWNARROW:    return SDL_SCANCODE_DOWN;
        case KEY_UPARROW:      return SDL_SCANCODE_UP;
        case KEY_ESCAPE:       return SDL_SCANCODE_ESCAPE;
        case KEY_ENTER:        return SDL_SCANCODE_RETURN;
        case KEY_TAB:          return SDL_SCANCODE_TAB;
        case KEY_F1:           return SDL_SCANCODE_F1;
        case KEY_F2:           return SDL_SCANCODE_F2;
        case KEY_F3:           return SDL_SCANCODE_F3;
        case KEY_F4:           return SDL_SCANCODE_F4;
        case KEY_F5:           return SDL_SCANCODE_F5;
        case KEY_F6:           return SDL_SCANCODE_F6;
        case KEY_F7:           return SDL_SCANCODE_F7;
        case KEY_F8:           return SDL_SCANCODE_F8;
        case KEY_F9:           return SDL_SCANCODE_F9;
        case KEY_F10:          return SDL_SCANCODE_F10;
        case KEY_F11:          return SDL_SCANCODE_F11;
        case KEY_F12:          return SDL_SCANCODE_F12;
        case KEY_BACKSPACE:    return SDL_SCANCODE_BACKSPACE;
        case KEY_DEL:          return SDL_SCANCODE_DELETE;
        case KEY_PAUSE:        return SDL_SCANCODE_PAUSE;
        case KEY_EQUALS:       return SDL_SCANCODE_EQUALS;
        case KEY_MINUS:        return SDL_SCANCODE_MINUS;
        case KEY_RSHIFT:       return SDL_SCANCODE_RSHIFT;
        case KEY_RCTRL:        return SDL_SCANCODE_RCTRL;
        case KEY_RALT:         return SDL_SCANCODE_RALT;
        case KEY_CAPSLOCK:     return SDL_SCANCODE_CAPSLOCK;
		case KEY_SCRLCK:       return SDL_SCANCODE_SCROLLLOCK;
		case KEYP_0:           return SDL_SCANCODE_KP_0;
        case KEYP_1:           return SDL_SCANCODE_KP_1;
        case KEYP_3:           return SDL_SCANCODE_KP_3;
        case KEYP_5:           return SDL_SCANCODE_KP_5;
        case KEYP_7:           return SDL_SCANCODE_KP_7;
        case KEYP_9:           return SDL_SCANCODE_KP_9;
        case KEYP_PERIOD:      return SDL_SCANCODE_KP_PERIOD;
        case KEYP_MULTIPLY:    return SDL_SCANCODE_KP_MULTIPLY;
        case KEYP_DIVIDE:      return SDL_SCANCODE_KP_DIVIDE;
        case KEY_INS:          return SDL_SCANCODE_INSERT;
		case KEY_PRINT:        return SDL_SCANCODE_PRINTSCREEN;
		case KEY_NUMLOCK:      return SDL_SCANCODE_NUMLOCKCLEAR;
        default:               return key;
    }
}

boolean keystate(int key)
{
	const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    return keystate[TranslateKey2(key)];
}

void I_SaveWindowPosition(void)
{
    if (!fullscreen)
    {
		if ( sdl_window != NULL )
		{
			static SDL_SysWMinfo pInfo;
			RECT r;

			SDL_VERSION(&pInfo.version);
			SDL_GetWindowWMInfo(sdl_window,&pInfo);

			GetWindowRect(pInfo.info.win.window, &r);
			sprintf(windowposition, "%i,%i", r.left + 8, r.top + 30);
		}
    }
}

void done_win32();

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        SetShowCursor(true);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        done_win32();

        initialized = false;
    }
}

static void UpdateMouseButtonState(unsigned int button, boolean on)
{
// AO: Disable mouse support
#if 0
    event_t ev;

    if (button < SDL_BUTTON_LEFT || button > MAX_MOUSE_BUTTONS)
        return;

    // Note: button "0" is left, button "1" is right,
    // button "2" is middle for Doom.  This is different
    // to how SDL sees things.

    switch (button)
    {
        case SDL_BUTTON_LEFT:
            button = 0;
            break;

        case SDL_BUTTON_RIGHT:
            button = 1;
            break;

        case SDL_BUTTON_MIDDLE:
            button = 2;
            break;

        default:
            // SDL buttons are indexed from 1.
            --button;
            break;
    }

    // Turn bit representing this button on or off.

    if (on)
        mouse_button_state |= (1 << button);
    else
        mouse_button_state &= ~(1 << button);

    // Post an event with the new button state.

    ev.type = ev_mouse;
    ev.data1 = mouse_button_state;
    ev.data2 = ev.data3 = 0;
    D_PostEvent(&ev);
#endif
}

static int AccelerateMouse(int val)
{
    if (val < 0)
        return -AccelerateMouse(-val);

    if (val > mouse_threshold)
        return (int)((val - mouse_threshold) * mouse_acceleration + mouse_threshold);
    else
        return val;
}

boolean altdown = false;
boolean waspaused = false;

void I_GetEvent(void)
{
    SDL_Event sdlevent;
    event_t   ev;

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                ev.type = ev_keydown;
                ev.data1 = TranslateKey(&sdlevent.key.keysym);
                ev.data2 = sdlevent.key.keysym.sym;

                altdown = (sdlevent.key.keysym.mod & KMOD_ALT);

                if (altdown && ev.data1 == KEY_TAB)
                    ev.data1 = ev.data2 = ev.data3 = 0;

				if ( ev.data2 >= 0 && ev.data2 <= 255 )
				{
	                if (!isdigit(ev.data2))
		                idclev = idmus = false;

					if (idbehold && keys[ev.data2])
					{
						HU_clearMessages();
						idbehold = false;
					}
				}

                if (ev.data1)
                    D_PostEvent(&ev);
                break;

            case SDL_KEYUP:
                ev.type = ev_keyup;
                ev.data1 = TranslateKey(&sdlevent.key.keysym);
                ev.data2 = 0;

                altdown = (sdlevent.key.keysym.mod & KMOD_ALT);
                keydown = 0;

                if (ev.data1 != 0)
                    D_PostEvent(&ev);
                break;

            case SDL_MOUSEBUTTONDOWN:
                {
                    if (window_focused)
                    {
                        idclev = false;
                        idmus = false;
                        if (idbehold)
                        {
                            HU_clearMessages();
                            idbehold = false;
                        }
                        UpdateMouseButtonState(sdlevent.button.button, true);
                    }
                    break;
                }

            case SDL_MOUSEBUTTONUP:
                {
                    if (window_focused)
                    {
                        keydown = 0;
                        UpdateMouseButtonState(sdlevent.button.button, false);
                    }
                    break;
                }

            case SDL_JOYBUTTONUP:
                keydown = 0;
                break;

            case SDL_QUIT:
                if (!quitting)
                {
                    keydown = 0;
                    if (paused)
                    {
                        paused = false;
                        waspaused = true;
                    }
                    S_StartSound(NULL, sfx_swtchn);
                    M_QuitDOOM();
                }
                break;
            case SDL_SYSWMEVENT:
				if (sdlevent.syswm.msg->msg.win.msg == WM_MOVE)
                {
                    I_SaveWindowPosition();
                    SetWindowPositionVars();
                    M_SaveDefaults();
                }
                break;
			case SDL_WINDOWEVENT:
				{
					switch (sdlevent.window.event)
					{
					case SDL_WINDOWEVENT_FOCUS_GAINED:
					case SDL_WINDOWEVENT_FOCUS_LOST:
						// need to update our focus state
						UpdateFocus();
						break;
					case SDL_WINDOWEVENT_EXPOSED:
						palette_to_set = true;
						break;
					case SDL_WINDOWEVENT_RESIZED:
						if (!fullscreen)
						{
							need_resize = true;
							resize_h = sdlevent.window.data2;
							break;
						}
						break;
					}
				}
				break;
            default:
                break;
        }
    }
}

// Warp the mouse back to the middle of the screen

static void CenterMouse(void)
{
    // Warp the the screen center

	SDL_WarpMouseInWindow( sdl_window, screen->w / 2, screen->h / 2);

    // Clear any relative movement caused by warping

    SDL_PumpEvents();
    SDL_GetRelativeMouseState(NULL, NULL);
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.

static void I_ReadMouse(void)
{
// AO: Disable mouse support
#if 0
    int     x, y;
    event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (x)
    {
        ev.type = ev_mouse;
        ev.data1 = mouse_button_state;
        ev.data2 = AccelerateMouse(x);
        ev.data3 = 0;

        D_PostEvent(&ev);
    }

    if (MouseShouldBeGrabbed())
        CenterMouse();
#endif
}

//
// I_StartTic
//
void I_StartTic(void)
{
    I_GetEvent();
    I_ReadMouse();
    gamepadfunc();
}

boolean currently_grabbed = false;

static void UpdateGrab(void)
{
    boolean grab = MouseShouldBeGrabbed();

    if (grab && !currently_grabbed)
    {
        SetShowCursor(false);
    }
    else if (!grab && currently_grabbed)
    {
        SetShowCursor(true);
		SDL_WarpMouseInWindow(sdl_window, screen->w - 16, screen->h - 16);
        SDL_GetRelativeMouseState(NULL, NULL);
    }

    currently_grabbed = grab;
}

static __forceinline void blit(void)
{
    fixed_t i = 0;
    fixed_t y = starty;

    do
    {
        byte    *dest = pixels + i;
        byte    *src = *screens + (y >> FRACBITS) * SCREENWIDTH;
        fixed_t x = startx;

        do
            *dest++ = *(src + (x >> FRACBITS));
        while ((x += stepx) < (SCREENWIDTH << FRACBITS));
        i += pitch;
    }
    while ((y += stepy) < (SCREENHEIGHT << FRACBITS));
}

SDL_Rect dest_rect;

//
// I_FinishUpdate
//
void I_FinishUpdate(void)
{
	const char *errorStr;
	static SDL_Texture *blah;

    if (need_resize)
    {
        ApplyWindowResize(resize_h);
        need_resize = false;
        palette_to_set = true;
    }

    UpdateGrab();

    // Don't update the screen if the window isn't visible.
    // Not doing this breaks under Windows when we alt-tab away
    // while fullscreen.

    if (!screenvisible)
        return;

    if (palette_to_set)
    {
		SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
		if ( sdl_texture != NULL )
		{
			SDL_DestroyTexture( sdl_texture );
		}
		sdl_texture = SDL_CreateTextureFromSurface( sdl_renderer, screenbuffer );
				
        //SDL_SetColors(screenbuffer, palette, 0, 256);
        palette_to_set = false;
    }

    // draw to screen
	errorStr = SDL_GetError();
#if 1
    if (SDL_LockSurface(screenbuffer) >= 0)
    {
        blit();
        SDL_UnlockSurface(screenbuffer);
    }

	SDL_BlitSurface( screenbuffer, NULL, screen, NULL );
	SDL_UpdateTexture(sdl_texture, NULL, screen->pixels, screen->pitch);
	SDL_RenderClear(sdl_renderer);
	SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);
#else
    if (SDL_LockSurface(screenbuffer) >= 0)
    {
        blit();
        SDL_UnlockSurface(screenbuffer);
    }

    SDL_FillRect(screen, NULL, 0);

//#error At this point, your 1.2 game had a bunch of SDL_Surfaces, which it would SDL_BlitSurface() to the screen surface to compose the final framebuffer, and eventually SDL_Flip() to the screen. For SDL 2.0, you have a bunch of SDL_Textures, that you will SDL_RenderCopy() to your Renderer to compose the final framebuffer, and eventually SDL_RenderPresent() to the screen. It's that simple. If these textures never need modification, you might find your framerate has just gone through the roof, too.
	// ao: test does this actually still work in SDL 2?
    //SDL_BlitSurface(screenbuffer, NULL, screen, &dest_rect);
	SDL_RenderCopy(sdl_renderer,sdl_texture,NULL,NULL);

//#error SDL_UpdateRect()/SDL_Flip(): use SDL_RenderPresent() instead
    SDL_Flip(screen);
	assert( false ); // ao: test
#endif
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
    memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette(byte *doompalette)
{
    int i;

    for (i = 0; i < 256; ++i)
    {
        palette[i].r = gammatable[usegamma][*doompalette++];
        palette[i].g = gammatable[usegamma][*doompalette++];
        palette[i].b = gammatable[usegamma][*doompalette++];
		palette[i].a = 255;
    }

    palette_to_set = true;
}

static void CreateCursors(void)
{
    static Uint8 emptycursordata = 0;

    emptycursor = SDL_CreateCursor(&emptycursordata, &emptycursordata, 1, 1, 0, 0);
}

static void SetWindowPositionVars(void)
{
    char buf[64];
    int x, y;

    if (sscanf(windowposition, "%i,%i", &x, &y) == 2)
    {
        if (x < 0)
            x = 0;
        else if (x > desktopwidth)
            x = desktopwidth - 16;
        if (y < 0)
            y = 0;
        else if (y > desktopheight)
            y = desktopheight - 16;
        sprintf(buf, "SDL_VIDEO_WINDOW_POS=%i,%i", x, y);
        putenv(buf);
    }
    else
    {
        putenv("SDL_VIDEO_CENTERED=1");
    }
}

static void SetVideoMode(void)
{
//#error SDL_VideoInfo: use SDL_GetRendererInfo()/SDL_GetRenderDriverInfo() instead
//    SDL_VideoInfo *videoinfo = (SDL_VideoInfo *)SDL_GetVideoInfo();
    //desktopwidth = videoinfo->current_w;
    //desktopheight = videoinfo->current_h;

	const char* errorStr;
	SDL_Rect displayBounds;

	SDL_GetDisplayBounds(0, &displayBounds);
	desktopwidth = displayBounds.w;
	desktopheight = displayBounds.h;

    if (fullscreen)
    {
        width = screenwidth;
        height = screenheight;
        if (!width || !height)
        {
            width = desktopwidth;
            height = desktopheight;
        }

//#error SDL_SetVideoMode -> change to "SDL_Window *screen = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL); https://wiki.libsdl.org/MigrationGuide
//        screen = SDL_SetVideoMode(width, height, 0,
//                                  SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
		if ( sdl_window != NULL )
		{
			SDL_DestroyWindow( sdl_window );
		}
		sdl_window = SDL_CreateWindow(gamedescription,
										SDL_WINDOWPOS_UNDEFINED,
										SDL_WINDOWPOS_UNDEFINED,
										width, height,
										SDL_WINDOW_FULLSCREEN);
		screen = SDL_GetWindowSurface(sdl_window);
		sdl_renderer = SDL_CreateRenderer(sdl_window,-1,SDL_RENDERER_PRESENTVSYNC);

        height = screen->h;
        width = height * 4 / 3;
        width += (width & 1);

        if (width > screen->w)
        {
            width = screen->w;
            height = width * 3 / 4;
            height += (height & 1);
        }
    }
    else
    {
        height = MAX(ORIGINALWIDTH * 3 / 4, windowheight);
        width = height * 4 / 3;
        width += (width & 1);

        if (width > windowwidth)
        {
            width = windowwidth;
            height = width * 3 / 4;
            height += (height & 1);
        }

        SetWindowPositionVars();
		sdl_window = SDL_CreateWindow(gamedescription,
										SDL_WINDOWPOS_UNDEFINED,
										SDL_WINDOWPOS_UNDEFINED,
										windowwidth, windowheight,
										SDL_WINDOW_RESIZABLE);
		screen = SDL_GetWindowSurface( sdl_window );
		sdl_renderer = SDL_CreateRenderer(sdl_window,-1,SDL_RENDERER_PRESENTVSYNC);
		//screen = SDL_SetVideoMode(windowwidth, windowheight, 0,
        //                          SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_RESIZABLE);

        widescreen = false;
    }

	screenbuffer = SDL_CreateRGBSurface(0, width, height, 8, 0, 0, 0, 0);
	sdl_texture = SDL_CreateTextureFromSurface( sdl_renderer, screenbuffer );
    pitch = screenbuffer->pitch;
    pixels = (byte *)screenbuffer->pixels;

    stepx = (SCREENWIDTH << FRACBITS) / width;
    stepy = (SCREENHEIGHT << FRACBITS) / height;

    startx = stepx - 1;
    starty = stepy - 1;

    dest_rect.x = (screen->w - screenbuffer->w) / 2;
    dest_rect.y = (screen->h - screenbuffer->h) / 2;

	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
	SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
	SDL_RenderClear(sdl_renderer);
	SDL_RenderPresent(sdl_renderer);

	errorStr = SDL_GetError();
}

void ToggleWideScreen(boolean toggle)
{
    if ((double)screen->w / screen->h < 1.6)
    {
        widescreen = returntowidescreen = false;
        return;
    }

    if (toggle)
    {
        if (!dest_rect.x && !dest_rect.y)
            return;

        widescreen = true;

        if (returntowidescreen && screenblocks == 11)
        {
            screenblocks = 10;
            R_SetViewSize(screenblocks);
        }

        width = screen->w;
        height = screen->h + (int)((double)screen->h * 32 / (ORIGINALHEIGHT - 32) + 1.5);
    }
    else
    {
        widescreen = false;

        height = screen->h;
        width = height * 4 / 3;
        width += (width & 1);
    }
    returntowidescreen = false;

    screenbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
    pitch = screenbuffer->pitch;
    pixels = (byte *)screenbuffer->pixels;

    stepx = (SCREENWIDTH << FRACBITS) / width;
    stepy = (SCREENHEIGHT << FRACBITS) / height;

    startx = stepx - 1;
    starty = stepy - 1;

    dest_rect.x = (screen->w - screenbuffer->w) / 2;
    dest_rect.y = (screen->h - screenbuffer->h) / 2;

    palette_to_set = true;
}

void init_win32(LPCTSTR lpIconName);

void ToggleFullScreen(void)
{
    initialized = false;

    fullscreen = !fullscreen;
    if (fullscreen)
    {
        width = screenwidth;
        height = screenheight;
        if (!width || !height)
        {
            width = desktopwidth;
            height = desktopheight;
        }

//#error SDL_SetVideoMode -> change to "SDL_Window *screen = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL); https://wiki.libsdl.org/MigrationGuide
//        screen = SDL_SetVideoMode(width, height, 0,
//                                  SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
		if ( sdl_window != NULL )
		{
			SDL_DestroyWindow( sdl_window );
		}
		sdl_window = SDL_CreateWindow(gamedescription,
										SDL_WINDOWPOS_UNDEFINED,
										SDL_WINDOWPOS_UNDEFINED,
										width, height,
										SDL_WINDOW_FULLSCREEN);
		screen = SDL_GetWindowSurface(sdl_window);


        if (screenblocks == 11)
        {
            if (gamestate != GS_LEVEL)
                returntowidescreen = true;
            else
            {
                dest_rect.x = 1;
                ToggleWideScreen(true);
                if (widescreen && screenblocks == 11)
                    screenblocks = 10;
                R_SetViewSize(screenblocks);
                initialized = true;
                M_SaveDefaults();
                return;
            }
        }

        height = screen->h;
        width = height * 4 / 3;
        width += (width & 1);

        if (width > screen->w)
        {
            width = screen->w;
            height = width * 3 / 4;
            height += (height & 1);
        }
    }
    else
    {
        event_t ev;

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        putenv(envstring);
        SDL_InitSubSystem(SDL_INIT_VIDEO);

        init_win32(gamemission == doom ? "doom" : "doom2");

        height = MAX(ORIGINALWIDTH * 3 / 4, windowheight);
        width = height * 4 / 3;
        width += (width & 1);

        if (width > windowwidth)
        {
            width = windowwidth;
            height = width * 3 / 4;
            height += (height & 1);
        }

        if (widescreen)
        {
            widescreen = false;
            screenblocks = 11;
            R_SetViewSize(screenblocks);
        }

        SetWindowPositionVars();
//#error SDL_SetVideoMode -> change to "SDL_Window *screen = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL); https://wiki.libsdl.org/MigrationGuide
//		screen = SDL_SetVideoMode(width, height, 0,
//                                  SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_RESIZABLE);
		if ( sdl_window != NULL )
		{
			SDL_DestroyWindow( sdl_window );
		}
		sdl_window = SDL_CreateWindow(gamestate == GS_LEVEL ? mapnumandtitle : gamedescription,
										SDL_WINDOWPOS_UNDEFINED,
										SDL_WINDOWPOS_UNDEFINED,
										width, height,
										SDL_WINDOW_RESIZABLE);
		screen = SDL_GetWindowSurface(sdl_window);

        CreateCursors();
        SDL_SetCursor(emptycursor);

//#error SDL_WM_SetCaption -> SDL_SetWindowTitle https://wiki.libsdl.org/SDL_SetWindowTitle
//        SDL_WM_SetCaption(, NULL);
//		SDL_SetWindowTitle(sdl_window, gamestate == GS_LEVEL ? mapnumandtitle : gamedescription );

        currently_grabbed = true;
        UpdateFocus();
        UpdateGrab();

		// ao: unnecessary?
        //SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

        ev.type = ev_keyup;
        ev.data1 = KEY_RALT;
        ev.data2 = ev.data3 = 0;
        D_PostEvent(&ev);
    }

    screenbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
    pitch = screenbuffer->pitch;
    pixels = (byte *)screenbuffer->pixels;

    stepx = (SCREENWIDTH << FRACBITS) / width;
    stepy = (SCREENHEIGHT << FRACBITS) / height;

    startx = stepx - 1;
    starty = stepy - 1;

    dest_rect.x = (screen->w - screenbuffer->w) / 2;
    dest_rect.y = (screen->h - screenbuffer->h) / 2;

    M_SaveDefaults();
    initialized = true;
}

void ApplyWindowResize(int height)
{
    windowheight = MAX(ORIGINALWIDTH * 3 / 4, height);
    windowwidth = windowheight * 4 / 3;
    windowwidth += (windowwidth & 1);

//#error SDL_SetVideoMode -> change to "SDL_Window *screen = SDL_CreateWindow("My Game Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL); https://wiki.libsdl.org/MigrationGuide
//    screen = SDL_SetVideoMode(windowwidth, windowheight, 0,
//                              SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_RESIZABLE);
	SDL_SetWindowSize(sdl_window, windowwidth, windowheight);
	screen = SDL_GetWindowSurface(sdl_window);

    screenbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, windowwidth, windowheight, 8, 0, 0, 0, 0);
    pitch = screenbuffer->pitch;
    pixels = (byte *)screenbuffer->pixels;

    stepx = (SCREENWIDTH << FRACBITS) / windowwidth;
    stepy = (SCREENHEIGHT << FRACBITS) / windowheight;

    startx = stepx - 1;
    starty = stepy - 1;

    dest_rect.x = (screen->w - screenbuffer->w) / 2;
    dest_rect.y = (screen->h - screenbuffer->h) / 2;

    M_SaveDefaults();
}

void I_InitGammaTables(void)
{
    int i;
    int j;

    for (i = 0; i < GAMMALEVELS; i++)
        if (gammalevel[i] == 1.0)
            for (j = 0; j < 256; j++)
                gammatable[i][j] = j;
        else
            for (j = 0; j < 256; j++)
                gammatable[i][j] = (byte)(pow((j + 1) / 256.0, 1.0 / gammalevel[i]) * 255.0);
}

void I_InitGraphics(void)
{
    int       i = 0;
    SDL_Event dummy;
    byte      *doompal = (byte *)W_CacheLumpName("PLAYPAL", PU_CACHE);
	const char* errorStr;

    while (i < UCHAR_MAX)
        keys[i++] = true;
    keys['v'] = keys['V'] = false;
    keys['s'] = keys['S'] = false;
    keys['i'] = keys['I'] = false;
    keys['r'] = keys['R'] = false;
    keys['a'] = keys['A'] = false;
    keys['l'] = keys['L'] = false;

    I_InitTintTables(doompal);

    I_InitGammaTables();

	// ao: SDL_VIDEODRIVER not required any more?
    //sprintf(envstring, "SDL_VIDEODRIVER=%s", videodriver);
    //putenv(envstring);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        I_Error("Failed to initialize video: %s", SDL_GetError());

    CreateCursors();
    SDL_SetCursor(emptycursor);

    SetVideoMode();

    init_win32(gamemission == doom ? "doom" : "doom2");

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

//#error SDL_WM_SetCaption -> SDL_SetWindowTitle https://wiki.libsdl.org/SDL_SetWindowTitle
//    SDL_WM_SetCaption(gamedescription, NULL);
	SDL_SetWindowTitle(sdl_window,gamedescription);

    SDL_FillRect(screenbuffer, NULL, 0);

    I_SetPalette(doompal);
    //SDL_SetColors(screenbuffer, palette, 0, 256);
	SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
	if ( sdl_texture != NULL )
	{
		SDL_DestroyTexture( sdl_texture );
	}
	sdl_texture = SDL_CreateTextureFromSurface( sdl_renderer, screenbuffer );

	errorStr = SDL_GetError();

    UpdateFocus();
    UpdateGrab();

    screens[0] = (byte *)Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

    memset(screens[0], 0, SCREENWIDTH * SCREENHEIGHT);

	// ao: unnecessary?
    //SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    while (SDL_PollEvent(&dummy));

    if (fullscreen)
        CenterMouse();

    initialized = true;
}
