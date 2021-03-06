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

#include "doomdef.h"
#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

typedef struct
{
    // Should be "IWAD" or "PWAD".
    char        identification[4];
    int         numlumps;
    int         infotableofs;
} PACKEDATTR wadinfo_t;

typedef struct
{
    int         filepos;
    int         size;
    char        name[8];
} PACKEDATTR filelump_t;

//
// GLOBALS
//

// Location of each lump on disk.

lumpinfo_t      *lumpinfo;
unsigned int    numlumps = 0;

// Hash table for fast lookups

static lumpinfo_t **lumphash;

static void ExtractFileBase(char *path, char *dest)
{
    char        *src;
    char        *filename;
    int         length;

    src = path + strlen(path) - 1;

    // back up until a \ or the start
    while (src != path && *(src - 1) != DIR_SEPARATOR)
        src--;

    // copy up to eight characters
    filename = src;
    length = 0;
    memset(dest, 0, 8);

    while (*src != '\0' && *src != '.')
        dest[length++] = toupper((int)*src++);
}

// Hash function used for lump names.

unsigned int W_LumpNameHash(const char *s)
{
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i = 0; i < 8 && s[i] != '\0'; ++i)
        result = ((result << 5) ^ result) ^ toupper(s[i]);

    return result;
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.

wad_file_t *W_AddFile (char *filename)
{
    wadinfo_t header;
    lumpinfo_t *lump_p;
    unsigned int i;
    wad_file_t *wad_file;
    int length;
    int startlump;
    filelump_t *fileinfo;
    filelump_t *filerover;

    // open the file and add to directory

    wad_file = W_OpenFile(filename);

    if (wad_file == NULL)
        return NULL;

    startlump = numlumps;

    if (strcasecmp(filename + strlen(filename) - 3, "wad"))
    {
        // single lump file

        // fraggle: Swap the filepos and size here.  The WAD directory
        // parsing code expects a little-endian directory, so will swap
        // them back.  Effectively we're constructing a "fake WAD directory"
        // here, as it would appear on disk.

        fileinfo = (filelump_t *)Z_Malloc(sizeof(filelump_t), PU_STATIC, 0);
        fileinfo->filepos = LONG(0);
        fileinfo->size = LONG(wad_file->length);

        // Name the lump after the base of the filename (without the
        // extension).

        ExtractFileBase(filename, fileinfo->name);
        numlumps++;
    }
    else
    {
        // WAD file
        W_Read(wad_file, 0, &header, sizeof(header));

        if (strncmp(header.identification, "IWAD", 4))
        {
            // Homebrew levels?
            if (strncmp(header.identification, "PWAD", 4))
                I_Error("Wad file %s doesn't have IWAD or PWAD id\n", filename);
        }

        header.numlumps = LONG(header.numlumps);
        header.infotableofs = LONG(header.infotableofs);
        length = header.numlumps * sizeof(filelump_t);
        fileinfo = (filelump_t *)Z_Malloc(length, PU_STATIC, 0);

        W_Read(wad_file, header.infotableofs, fileinfo, length);
        numlumps += header.numlumps;
    }

    // Fill in lumpinfo
    lumpinfo = (lumpinfo_t *)realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));

    if (lumpinfo == NULL)
        I_Error("Couldn't realloc lumpinfo");

    lump_p = &lumpinfo[startlump];

    filerover = fileinfo;

    for (i = startlump; i < numlumps; ++i)
    {
        lump_p->wad_file = wad_file;
        lump_p->position = LONG(filerover->filepos);
        lump_p->size = LONG(filerover->size);
        lump_p->cache = NULL;
        strncpy(lump_p->name, filerover->name, 8);

        ++lump_p;
        ++filerover;
    }

    Z_Free(fileinfo);

    if (lumphash != NULL)
    {
        Z_Free(lumphash);
        lumphash = NULL;
    }

    return wad_file;
}

//
// W_WadType
// Returns IWAD, PWAD or 0.
//
int W_WadType(char *filename)
{
    wadinfo_t  header;
    wad_file_t *wad_file = W_OpenFile(filename);

    if (!wad_file)
        return 0;

    W_Read(wad_file, 0, &header, sizeof(header));

    if (!strncmp(header.identification, "IWAD", 4))
        return IWAD;
    else if (!strncmp(header.identification, "PWAD", 4))
        return PWAD;
    else
        return 0;
}

//
// W_NumLumps
//
int W_NumLumps(void)
{
    return numlumps;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//

int W_CheckNumForName(char *name)
{
    lumpinfo_t  *lump_p;
    int         i;

    // Do we have a hash table yet?

    if (lumphash != NULL)
    {
        int     hash;

        // We do! Excellent.

        hash = W_LumpNameHash(name) % numlumps;

        for (lump_p = lumphash[hash]; lump_p != NULL; lump_p = lump_p->next)
            if (!strncasecmp(lump_p->name, name, 8))
                return lump_p - lumpinfo;
    }
    else
    {
        // We don't have a hash table generate yet. Linear search :-(
        //
        // scan backwards so patch lump files take precedence

        for (i = numlumps - 1; i >= 0; --i)
            if (!strncasecmp(lumpinfo[i].name, name, 8))
                return i;
    }

    // TFB. Not found.
    return -1;
}

//
// W_CheckMultipleLumps
// Check if there's more than one of the same lump.
//
int W_CheckMultipleLumps(char *name)
{
    int         i;
    int         count = 0;

    for (i = numlumps - 1; i >= 0; --i)
        if (!strncasecmp(lumpinfo[i].name, name, 8))
            ++count;

    return count;
}

//
// W_RangeCheckNumForName
// Linear Search that checks for a lump number ONLY
// inside a range, not all lumps.
//
int W_RangeCheckNumForName(int min, int max, char *name)
{
    int         i;

    for (i = min; i <= max; i++)
        if (!strncasecmp(lumpinfo[i].name, name, 8))
            return i;

    I_Error("W_RangeCheckNumForName: %s not found!", name);
}

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName(char *name)
{
    int         i;

    i = W_CheckNumForName(name);

    if (i < 0)
        I_Error("W_GetNumForName: %s not found!", name);

    return i;
}

// Go forwards rather than backwards so we get lump from IWAD and not PWAD
int W_GetNumForName2(char *name)
{
    unsigned int i;

    for (i = 0; i < numlumps; i++)
        if (!strncasecmp(lumpinfo[i].name, name, 8))
            break;

    if (i == numlumps)
        I_Error("W_GetNumForName: %s not found!", name);

    return i;
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength(unsigned int lump)
{
    if (lump >= numlumps)
        I_Error("W_LumpLength: %i >= numlumps", lump);

    return lumpinfo[lump].size;
}

//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void W_ReadLump(unsigned int lump, void *dest)
{
    int         c;
    lumpinfo_t  *l;

    if (lump >= numlumps)
        I_Error("W_ReadLump: %i >= numlumps", lump);

    l = lumpinfo + lump;

    c = W_Read(l->wad_file, l->position, dest, l->size);

    if (c < l->size)
        I_Error("W_ReadLump: only read %i of %i on lump %i", c, l->size, lump);
}

//
// W_CacheLumpNum
//
// Load a lump into memory and return a pointer to a buffer containing
// the lump data.
//
// 'tag' is the type of zone memory buffer to allocate for the lump
// (usually PU_STATIC or PU_CACHE).  If the lump is loaded as
// PU_STATIC, it should be released back using W_ReleaseLumpNum
// when no longer needed (do not use Z_ChangeTag).
//
void *W_CacheLumpNum(int lumpnum, int tag)
{
    byte        *result;
    lumpinfo_t  *lump;

    if ((unsigned)lumpnum >= numlumps)
        I_Error("W_CacheLumpNum: %i >= numlumps", lumpnum);

    lump = &lumpinfo[lumpnum];

    // Get the pointer to return.  If the lump is in a memory-mapped
    // file, we can just return a pointer to within the memory-mapped
    // region.  If the lump is in an ordinary file, we may already
    // have it cached; otherwise, load it into memory.

    if (lump->wad_file->mapped != NULL)
    {
        // Memory mapped file, return from the mmapped region.
        result = lump->wad_file->mapped + lump->position;
    }
    else if (lump->cache != NULL)
    {
        // Already cached, so just switch the zone tag.
        result = (byte *)lump->cache;
        Z_ChangeTag(lump->cache, tag);
    }
    else
    {
        // Not yet loaded, so load it now
        lump->cache = Z_Malloc(W_LumpLength(lumpnum), tag, &lump->cache);
        W_ReadLump(lumpnum, lump->cache);
        result = (byte *)lump->cache;
    }

    return result;
}

//
// W_CacheLumpName
//
void *W_CacheLumpName(char *name, int tag)
{
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}

//
// Release a lump back to the cache, so that it can be reused later
// without having to read from disk again, or alternatively, discarded
// if we run out of memory.
//
// Back in Vanilla Doom, this was just done using Z_ChangeTag
// directly, but now that we have WAD mmap, things are a bit more
// complicated ...
//
void W_ReleaseLumpNum(int lumpnum)
{
    lumpinfo_t  *lump;

    if ((unsigned)lumpnum >= numlumps)
        I_Error("W_ReleaseLumpNum: %i >= numlumps", lumpnum);

    lump = &lumpinfo[lumpnum];

    if (!lump->wad_file->mapped)
        Z_ChangeTag(lump->cache, PU_CACHE);
}

void W_ReleaseLumpName(char *name)
{
    W_ReleaseLumpNum(W_GetNumForName(name));
}

// Generate a hash table for fast lookups
void W_GenerateHashTable(void)
{
    unsigned int i;

    // Free the old hash table, if there is one

    if (lumphash != NULL)
        Z_Free(lumphash);

    // Generate hash table
    if (numlumps > 0)
    {
        lumphash = (lumpinfo_t **)Z_Malloc(sizeof(lumpinfo_t *) * numlumps, PU_STATIC, NULL);
        memset(lumphash, 0, sizeof(lumpinfo_t *) * numlumps);

        for (i = 0; i < numlumps; ++i)
        {
            unsigned int hash;

            hash = W_LumpNameHash(lumpinfo[i].name) % numlumps;

            // Hook into the hash table

            lumpinfo[i].next = lumphash[hash];
            lumphash[hash] = &lumpinfo[i];
        }
    }

    // All done!
}
