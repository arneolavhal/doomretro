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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "i_system.h"
#include "m_misc.h"

int             myargc;
char            **myargv;




//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
//

int M_CheckParmWithArgs(char *check, int num_args)
{
    int i;

    for (i = 1; i < myargc - num_args; i++)
    {
        if (!strcasecmp(check, myargv[i]))
            return i;
    }

    return 0;
}

int M_CheckParm(char *check)
{
    return M_CheckParmWithArgs(check, 0);
}

#define MAXARGVS        100

static void LoadResponseFile(int argv_index)
{
    FILE *handle;
    int size;
    char *infile;
    char *file;
    char *response_filename;
    char **newargv;
    int newargc;
    int i, k;

    response_filename = myargv[argv_index] + 1;

    // Read the response file into memory
    handle = fopen(response_filename, "rb");

    if (handle == NULL)
    {
        exit(1);
    }

    size = M_FileLength(handle);

    // Read in the entire file
    // Allocate one byte extra - this is in case there is an argument
    // at the end of the response file, in which case a '\0' will be
    // needed.

    file = (char *)malloc(size + 1);

    if (fread(file, 1, size, handle) < (unsigned)size)
    {
        I_Error("Failed to read entire response file");
    }

    fclose(handle);

    // Create new arguments list array

    newargv = (char **)malloc(sizeof(char *) * MAXARGVS);
    newargc = 0;
    memset(newargv, 0, sizeof(char *) * MAXARGVS);

    // Copy all the arguments in the list up to the response file

    for (i = 0; i < argv_index; ++i)
    {
        newargv[i] = myargv[i];
        ++newargc;
    }

    infile = file;
    k = 0;

    while (k < size)
    {
        // Skip past space characters to the next argument

        while (k < size && isspace(infile[k]))
        {
            ++k;
        }

        if (k >= size)
        {
            break;
        }

        // If the next argument is enclosed in quote marks, treat
        // the contents as a single argument.  This allows long filenames
        // to be specified.

        if (infile[k] == '\"')
        {
            // Skip the first character(")
            ++k;

            newargv[newargc++] = &infile[k];

            // Read all characters between quotes

            while (k < size && infile[k] != '\"' && infile[k] != '\n')
            {
                ++k;
            }

            if (k >= size || infile[k] == '\n')
            {
                I_Error("Quotes unclosed in response file '%s'",
                        response_filename);
            }

            // Cut off the string at the closing quote

            infile[k] = '\0';
            ++k;
        }
        else
        {
            // Read in the next argument until a space is reached

            newargv[newargc++] = &infile[k];

            while (k < size && !isspace(infile[k]))
            {
                ++k;
            }

            // Cut off the end of the argument at the first space

            infile[k] = '\0';

            ++k;
        }
    }

    // Add arguments following the response file argument

    for (i = argv_index + 1; i < myargc; ++i)
    {
        newargv[newargc] = myargv[i];
        ++newargc;
    }

    myargv = newargv;
    myargc = newargc;
}

//
// Find a Response File
//

void M_FindResponseFile(void)
{
    int         i;

    for (i = 1; i < myargc; i++)
    {
        if (myargv[i][0] == '@')
        {
            LoadResponseFile(i);
        }
    }
}