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

#include <Shlobj.h>

#include "doomstat.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"


//
// DEFAULTS
//

// Location where all configuration data is stored -
// default.cfg, savegames, etc.

char *configdir;


extern int      key_right;
extern int      key_left;
extern int      key_up;
extern int      key_up2;
extern int      key_down;
extern int      key_down2;

extern int      key_strafeleft;
extern int      key_straferight;

extern int      key_fire;
extern int      key_use;
extern int      key_strafe;
extern int      key_speed;

extern int      key_nextweapon;
extern int      key_prevweapon;

extern int      mousebfire;

extern int      gamepadautomap;
extern int      gamepadfire;
extern int      gamepadmenu;
extern int      gamepadnextweapon;
extern int      gamepadprevweapon;
extern int      gamepadspeed;
extern int      gamepaduse;
extern int      gamepadweapon1;
extern int      gamepadweapon2;
extern int      gamepadweapon3;
extern int      gamepadweapon4;
extern int      gamepadweapon5;
extern int      gamepadweapon6;
extern int      gamepadweapon7;

extern int      gamepadlefthanded;
extern int      gamepadvibrate;

extern int      viewwidth;
extern int      viewheight;

extern int      mouseSensitivity;
extern int      showMessages;


extern boolean grid;
extern int selectedepisode;
extern int selectedexpansion;
extern int selectedskilllevel;
extern int selectedsavegame;

extern char *windowposition;

extern int fullscreen;
extern int windowwidth;
extern int windowheight;
extern int screenwidth;
extern int screenheight;
extern int widescreen;
extern char *videodriver;
extern int usegamma;

extern float mouse_acceleration;
extern int mouse_threshold;

typedef enum
{
    DEFAULT_INT,
    DEFAULT_INT_HEX,
    DEFAULT_STRING,
    DEFAULT_FLOAT,
    DEFAULT_KEY,
} default_type_t;

typedef struct
{
    // Name of the variable
    char *name;

    // Pointer to the location in memory of the variable
    void *location;

    // Type of the variable
    default_type_t type;

    // If this is a key value, the original integer scancode we read from
    // the config file before translating it to the internal key value.
    // If zero, we didn't read this value from a config file.
    int untranslated;

    // The value we translated the scancode into when we read the
    // config file on startup.  If the variable value is different from
    // this, it has been changed and needs to be converted; otherwise,
    // use the 'untranslated' value.
    int original_translated;

    int set;
} default_t;

typedef struct
{
    default_t *defaults;
    int numdefaults;
    char *filename;
} default_collection_t;

typedef struct
{
    char *text;
    int value;
    int set;
} alias_t;

#define CONFIG_VARIABLE_GENERIC(name, variable, type, set) \
    { #name, &variable, type, 0, 0, set }

#define CONFIG_VARIABLE_KEY(name, variable, set) \
    CONFIG_VARIABLE_GENERIC(name, variable, DEFAULT_KEY, set)
#define CONFIG_VARIABLE_INT(name, variable, set) \
    CONFIG_VARIABLE_GENERIC(name, variable, DEFAULT_INT, set)
#define CONFIG_VARIABLE_INT_HEX(name, variable, set) \
    CONFIG_VARIABLE_GENERIC(name, variable, DEFAULT_INT_HEX, set)
#define CONFIG_VARIABLE_FLOAT(name, variable, set) \
    CONFIG_VARIABLE_GENERIC(name, variable, DEFAULT_FLOAT, set)
#define CONFIG_VARIABLE_STRING(name, variable, set) \
    CONFIG_VARIABLE_GENERIC(name, variable, DEFAULT_STRING, set)

//! @begin_config_file default.cfg

static default_t        doom_defaults_list[] =
{
    CONFIG_VARIABLE_INT   (mouse_sensitivity,  mouseSensitivity,   0),
    CONFIG_VARIABLE_FLOAT (mouse_acceleration, mouse_acceleration, 0),
    CONFIG_VARIABLE_INT   (mouse_threshold,    mouse_threshold,    0),
    CONFIG_VARIABLE_INT   (sfx_volume,         sfxVolume,          0),
    CONFIG_VARIABLE_INT   (music_volume,       musicVolume,        0),
    CONFIG_VARIABLE_INT   (show_messages,      showMessages,       1),
    CONFIG_VARIABLE_KEY   (key_right,          key_right,          3),
    CONFIG_VARIABLE_KEY   (key_left,           key_left,           3),
    CONFIG_VARIABLE_KEY   (key_up,             key_up,             3),
    CONFIG_VARIABLE_KEY   (key_up2,            key_up2,            3),
    CONFIG_VARIABLE_KEY   (key_down,           key_down,           3),
    CONFIG_VARIABLE_KEY   (key_down2,          key_down2,          3),
    CONFIG_VARIABLE_KEY   (key_strafeleft,     key_strafeleft,     3),
    CONFIG_VARIABLE_KEY   (key_straferight,    key_straferight,    3),
    CONFIG_VARIABLE_KEY   (key_fire,           key_fire,           3),
    CONFIG_VARIABLE_KEY   (key_use,            key_use,            3),
    CONFIG_VARIABLE_KEY   (key_strafe,         key_strafe,         3),
    CONFIG_VARIABLE_KEY   (key_speed,          key_speed,          3),
    CONFIG_VARIABLE_KEY   (key_prevweapon,     key_prevweapon,     3),
    CONFIG_VARIABLE_KEY   (key_nextweapon,     key_nextweapon,     3),
    CONFIG_VARIABLE_INT   (mouse_fire,         mousebfire,         4),
    CONFIG_VARIABLE_INT   (gamepad_automap,    gamepadautomap,     2),
    CONFIG_VARIABLE_INT   (gamepad_fire,       gamepadfire,        2),
    CONFIG_VARIABLE_INT   (gamepad_menu,       gamepadmenu,        2),
    CONFIG_VARIABLE_INT   (gamepad_nextweapon, gamepadnextweapon,  2),
    CONFIG_VARIABLE_INT   (gamepad_prevweapon, gamepadprevweapon,  2),
    CONFIG_VARIABLE_INT   (gamepad_speed,      gamepadspeed,       2),
    CONFIG_VARIABLE_INT   (gamepad_use,        gamepaduse,         2),
    CONFIG_VARIABLE_INT   (gamepad_weapon1,    gamepadweapon1,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon2,    gamepadweapon2,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon3,    gamepadweapon3,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon4,    gamepadweapon4,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon5,    gamepadweapon5,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon6,    gamepadweapon6,     2),
    CONFIG_VARIABLE_INT   (gamepad_weapon7,    gamepadweapon7,     2),
    CONFIG_VARIABLE_INT   (gamepad_lefthanded, gamepadlefthanded,  1),
    CONFIG_VARIABLE_INT   (gamepad_vibrate,    gamepadvibrate,     1),
    CONFIG_VARIABLE_INT   (screenblocks,       screenblocks,       0),
    CONFIG_VARIABLE_INT   (usegamma,           usegamma,           1),
    CONFIG_VARIABLE_INT   (grid,               grid,               1),
    CONFIG_VARIABLE_INT   (episode,            selectedepisode,    0),
    CONFIG_VARIABLE_INT   (expansion,          selectedexpansion,  0),
    CONFIG_VARIABLE_INT   (skilllevel,         selectedskilllevel, 0),
    CONFIG_VARIABLE_INT   (savegame,           selectedsavegame,   0),
    CONFIG_VARIABLE_STRING(windowposition,     windowposition,     0),
    CONFIG_VARIABLE_INT   (windowwidth,        windowwidth,        0),
    CONFIG_VARIABLE_INT   (windowheight,       windowheight,       0),
    CONFIG_VARIABLE_INT   (fullscreen,         fullscreen,         1),
    CONFIG_VARIABLE_INT   (screenwidth,        screenwidth,        5),
    CONFIG_VARIABLE_INT   (screenheight,       screenheight,       5),
    CONFIG_VARIABLE_INT   (widescreen,         widescreen,         1),
    CONFIG_VARIABLE_STRING(videodriver,        videodriver,        0)
};

static default_collection_t doom_defaults =
{
    doom_defaults_list,
    arrlen(doom_defaults_list),
    NULL,
};

static const int scantokey[128] =
{
    0,             27,             '1',           '2',
    '3',           '4',            '5',           '6',
    '7',           '8',            '9',           '0',
    '-',           '=',            KEY_BACKSPACE, 9,
    'q',           'w',            'e',           'r',
    't',           'y',            'u',           'i',
    'o',           'p',            '[',           ']',
    13,            KEY_RCTRL,      'a',           's',
    'd',           'f',            'g',           'h',
    'j',           'k',            'l',           ';',
    '\'',          '`',            KEY_RSHIFT,    '\\',
    'z',           'x',            'c',           'v',
    'b',           'n',            'm',           ',',
    '.',           '/',            KEY_RSHIFT,    KEYP_MULTIPLY,
    KEY_RALT,      ' ',            KEY_CAPSLOCK,  KEY_F1,
    KEY_F2,        KEY_F3,         KEY_F4,        KEY_F5,
    KEY_F6,        KEY_F7,         KEY_F8,        KEY_F9,
    KEY_F10,       KEY_PAUSE,      KEY_SCRLCK,    KEY_HOME,
    KEY_UPARROW,   KEY_PGUP,       KEY_MINUS,     KEY_LEFTARROW,
    KEYP_5,        KEY_RIGHTARROW, KEYP_PLUS,     KEY_END,
    KEY_DOWNARROW, KEY_PGDN,       KEY_INS,       KEY_DEL,
    0,             0,              0,             KEY_F11,
    KEY_F12,       0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0,
    0,             0,              0,             0
};

static alias_t alias[] =
{
    { "false",             0,  1 },
    { "none",              0,  2 },
    { "none",              0,  3 },
    { "left",              0,  4 },
    { "desktop",           0,  5 },
    { "true",              1,  1 },
    { "dpadup",            1,  2 },
    { "right",             1,  4 },
    { "dpaddown",          2,  2 },
    { "middle",            2,  4 },
    { "dpadleft",          4,  2 },
    { "dpadright",         8,  2 },
    { "backspace",        14,  3 },
    { "start",            16,  2 },
    { "enter",            28,  3 },
    { "ctrl",             29,  3 },
    { "back",             32,  2 },
    { "shift",            42,  3 },
    { "alt",              56,  3 },
    { "space",            57,  3 },
    { "leftthumb",        64,  2 },
    { "home",             71,  3 },
    { "up",               72,  3 },
    { "pageup",           73,  3 },
    { "left",             75,  3 },
    { "right",            77,  3 },
    { "end",              79,  3 },
    { "down",             80,  3 },
    { "pagedown",         81,  3 },
    { "insert",           82,  3 },
    { "del",              83,  3 },
    { "rightthumb",      128,  2 },
    { "leftshoulder",    256,  2 },
    { "rightshoulder",   512,  2 },
    { "lefttrigger",    1024,  2 },
    { "righttrigger",   2048,  2 },
    { "a",              4096,  2 },
    { "b",              8192,  2 },
    { "x",             16384,  2 },
    { "y",             32768,  2 },
    { "",                 -1, -1 }
};

static void SaveDefaultCollection(default_collection_t *collection)
{
    default_t *defaults;
    int i, v;
    FILE *f;

    f = fopen(collection->filename, "w");
    if (!f)
        return; // can't write the file, but don't complain

    defaults = collection->defaults;

    for (i = 0; i < collection->numdefaults; i++)
    {
        int chars_written;

        // Print the name and line up all values at 30 characters

        chars_written = fprintf(f, "%s ", defaults[i].name);

        for (; chars_written < 30; ++chars_written)
            fprintf(f, " ");

        // Print the value

        switch (defaults[i].type)
        {
            case DEFAULT_KEY:

                // use the untranslated version if we can, to reduce
                // the possibility of screwing up the user's config
                // file

                v = *(int *)defaults[i].location;

                if (defaults[i].untranslated
                    && v == defaults[i].original_translated)
                {
                    // Has not been changed since the last time we
                    // read the config file.
                    int j = 0;
                    bool flag = false;

                    v = defaults[i].untranslated;
                    while (alias[j].value != -1)
                    {
                        if (v == alias[j].value && defaults[i].set == alias[j].set)
                        {
                            fprintf(f, "%s", alias[j].text);
                            flag = true;
                            break;
                        }
                        j++;
                    }
                    if (flag)
                        break;

                    if (isprint(scantokey[v]))
                        fprintf(f, "\'%c\'", scantokey[v]);
                    else
                        fprintf(f, "%i", v);
                }
                else
                {
                    // search for a reverse mapping back to a scancode
                    // in the scantokey table

                    int s;
                    for (s = 0; s < 128; ++s)
                    {
                        if (scantokey[s] == v)
                        {
                            int j = 0;
                            bool flag = false;

                            v = s;
                            while (alias[j].value != -1)
                            {
                                if (v == alias[j].value && defaults[i].set == alias[j].set)
                                {
                                    fprintf(f, "%s", alias[j].text);
                                    flag = true;
                                    break;
                                }
                                j++;
                            }
                            if (flag)
                                break;

                            if (isprint(scantokey[v]))
                                fprintf(f, "\'%c\'", scantokey[v]);
                            else
                                fprintf(f, "%i", v);
                            break;
                        }
                    }
                }

                break;

            case DEFAULT_INT:
                {
                    int j = 0;
                    bool flag = false;

                    v = *(int *)defaults[i].location;
                    while (alias[j].value != -1)
                    {
                        if (v == alias[j].value && defaults[i].set == alias[j].set)
                        {
                            fprintf(f, "%s", alias[j].text);
                            flag = true;
                            break;
                        }
                        j++;
                    }
                    if (!flag)
                        fprintf(f, "%i", *(int *)defaults[i].location);
                    break;
                }

            case DEFAULT_INT_HEX:
                fprintf(f, "0x%x", *(int *)defaults[i].location);
                break;

            case DEFAULT_FLOAT:
                fprintf(f, "%.1f", *(float *)defaults[i].location);
                break;

            case DEFAULT_STRING:
                fprintf(f, "\"%s\"", *(char **)defaults[i].location);
                break;
        }

        fprintf(f, "\n");
    }

    fclose(f);
}

// Parses integer values in the configuration file

static int ParseIntParameter(char *strparm, int set)
{
    int parm;

    if (strparm[0] == '\'' && strparm[2] == '\'')
    {
        int s;

        for (s = 0; s < 128; ++s)
        {
            if (tolower(strparm[1]) == scantokey[s])
            {
                return s;
            }
        }
    }
    else
    {
        int i = 0;

        while (alias[i].value != -1)
        {
            if (!strcasecmp(strparm, alias[i].text) && set == alias[i].set)
            {
                return alias[i].value;
            }
            i++;
        }
    }

    if (strparm[0] == '0' && strparm[1] == 'x')
        sscanf(strparm + 2, "%x", &parm);
    else
        sscanf(strparm, "%i", &parm);

    return parm;
}

static void LoadDefaultCollection(default_collection_t *collection)
{
    default_t *defaults = collection->defaults;
    int i;
    FILE *f;
    char defname[80];
    char strparm[100];

    // read the file in, overriding any set defaults
    f = fopen(collection->filename, "r");

    if (!f)
    {
        // File not opened, but don't complain

        return;
    }

    while (!feof(f))
    {
        if (fscanf(f, "%79s %[^\n]\n", defname, strparm) != 2)
        {
            // This line doesn't match

            continue;
        }

        // Strip off trailing non-printable characters (\r characters
        // from DOS text files)

        while (strlen(strparm) > 0 && !isprint(strparm[strlen(strparm) - 1]))
        {
            strparm[strlen(strparm) - 1] = '\0';
        }

        // Find the setting in the list

        for (i = 0; i < collection->numdefaults; ++i)
        {
            default_t *def = &collection->defaults[i];
            char *s;
            int intparm;

            if (strcmp(defname, def->name) != 0)
            {
                // not this one
                continue;
            }

            // parameter found

            switch (def->type)
            {
                case DEFAULT_STRING:
                    s = strdup(strparm + 1);
                    s[strlen(s) - 1] = '\0';
                    *(char **)def->location = s;
                    break;

                case DEFAULT_INT:
                case DEFAULT_INT_HEX:
                    *(int *)def->location = ParseIntParameter(strparm, def->set);
                    break;

                case DEFAULT_KEY:

                    // translate scancodes read from config
                    // file (save the old value in untranslated)

                    intparm = ParseIntParameter(strparm, def->set);
                    defaults[i].untranslated = intparm;
                    if (intparm >= 0 && intparm < 128)
                    {
                        intparm = scantokey[intparm];
                    }
                    else
                    {
                        intparm = 0;
                    }

                    defaults[i].original_translated = intparm;
                    *(int *)def->location = intparm;
                    break;

                case DEFAULT_FLOAT:
                    *(float *)def->location = (float)atof(strparm);
                    break;
            }

            // finish

            break;
        }
    }

    fclose(f);
}

//
// M_SaveDefaults
//

void M_SaveDefaults(void)
{
    SaveDefaultCollection(&doom_defaults);
}


//
// M_LoadDefaults
//

void M_LoadDefaults(void)
{
    int i;

    // check for a custom default file

    //!
    // @arg <file>
    // @vanilla
    //
    // Load configuration from the specified file, instead of
    // default.cfg.
    //

    i = M_CheckParmWithArgs("-config", 1);

    if (i)
    {
        doom_defaults.filename = myargv[i + 1];
    }
    else
    {
        doom_defaults.filename = (char *)malloc(strlen(configdir) + 20);
        sprintf(doom_defaults.filename, "%sdefault.cfg", configdir);
    }

    LoadDefaultCollection(&doom_defaults);

    // [BH] If there's a default.cfg file in the game folder, then use it instead.
    if (M_FileExists("./default.cfg"))
    {
        sprintf(doom_defaults.filename, "./default.cfg");
        LoadDefaultCollection(&doom_defaults);
    }
}

//
// SetConfigDir:
//
// Sets the location of the configuration directory, where configuration
// files are stored - default.cfg, savegames, etc.
//

void M_SetConfigDir(void)
{
    char appdata[MAX_PATH];

    HRESULT hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata);

    sprintf(appdata, "%s\\DOOM RETRO\\", appdata);

    M_MakeDirectory(appdata);
    configdir = strdup(appdata);
}