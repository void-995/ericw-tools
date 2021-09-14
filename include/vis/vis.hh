/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// vis.h

#include <common/cmdlib.hh>
#include <common/mathlib.hh>
#include <common/bspfile.hh>
#include <vis/leafbits.hh>

constexpr char *PORTALFILE = "PRT1";
constexpr char *PORTALFILE2 = "PRT2";
constexpr char *PORTALFILEAM = "PRT1-AM";
#define ON_EPSILON 0.1
#define EQUAL_EPSILON 0.001

#define MAX_WINDING 64
#define MAX_WINDING_FIXED 24

struct winding_t
{
    int numpoints;
    vec3_t origin; // Bounding sphere for fast clipping tests
    vec_t radius; // Not updated, so won't shrink when clipping
    vec3_t points[MAX_WINDING_FIXED]; // variable sized
};

winding_t *NewWinding(int points);
winding_t *CopyWinding(const winding_t *w);
void PlaneFromWinding(const winding_t *w, plane_t *plane);
qboolean PlaneCompare(plane_t *p1, plane_t *p2);

enum pstatus_t
{
    pstat_none = 0,
    pstat_working,
    pstat_done
};

struct portal_t
{
    plane_t plane; // normal pointing into neighbor
    int leaf; // neighbor
    winding_t *winding;
    pstatus_t status;
    leafbits_t *visbits;
    leafbits_t *mightsee;
    int nummightsee;
    int numcansee;
};

struct sep_t
{
    sep_t *next;
    plane_t plane; // from portal is on positive side
};

struct passage_t
{
    passage_t *next;
    int from, to; // leaf numbers
    sep_t *planes;
};

/* Increased MAX_PORTALS_ON_LEAF from 128 */
#define MAX_PORTALS_ON_LEAF 512

struct leaf_t
{
    int numportals;
    passage_t *passages;
    portal_t *portals[MAX_PORTALS_ON_LEAF];
    int visofs; // used when writing final visdata
};

#define MAX_SEPARATORS MAX_WINDING
#define STACK_WINDINGS 3 // source, pass and a temp for clipping

struct pstack_t
{
    pstack_t *next;
    leaf_t *leaf;
    portal_t *portal; // portal exiting
    winding_t *source, *pass;
    winding_t windings[STACK_WINDINGS]; // Fixed size windings
    int freewindings[STACK_WINDINGS];
    plane_t portalplane;
    leafbits_t *mightsee; // bit string
    plane_t separators[2][MAX_SEPARATORS]; /* Separator cache */
    int numseparators[2];
};

winding_t *AllocStackWinding(pstack_t *stack);
void FreeStackWinding(winding_t *w, pstack_t *stack);
winding_t *ClipStackWinding(winding_t *in, pstack_t *stack, plane_t *split);

struct threaddata_t
{
    leafbits_t *leafvis;
    portal_t *base;
    pstack_t pstack_head;
};

extern int numportals;
extern int portalleafs;
extern int portalleafs_real;
extern int *clustermap;

extern portal_t *portals;
extern leaf_t *leafs;

extern int c_noclip;
extern int c_portaltest, c_portalpass, c_portalcheck;
extern int c_vistest, c_mighttest;
extern unsigned long c_chains;

extern qboolean showgetleaf;

extern int testlevel;
extern qboolean ambientsky;
extern qboolean ambientwater;
extern qboolean ambientslime;
extern qboolean ambientlava;
extern int visdist;
extern qboolean nostate;

extern uint8_t *uncompressed;
extern int leafbytes;
extern int leafbytes_real;
extern int leaflongs;

extern char sourcefile[1024];
extern char portalfile[1024];
extern char statefile[1024];
extern char statetmpfile[1024];

void BasePortalVis(void);

void PortalFlow(portal_t *p);

void CalcAmbientSounds(mbsp_t *bsp);

extern double starttime, endtime, statetime;

void SaveVisState(void);
qboolean LoadVisState(void);

/* Print winding/leaf info for debugging */
void LogWinding(const winding_t *w);
void LogLeaf(const leaf_t *leaf);
