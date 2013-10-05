#include "region.h"
#include "atlantis.h"
#include "unit.h"
#include "ship.h"
#include "building.h"
#include "rtl.h"
#include "parser.h"

#include <quicklist.h>
#include <mtrand.h>

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

quicklist *regions;
#define RMAXHASH  (1<<12)
#define RHASHMASK (RMAXHASH-1)
struct region *regionhash[RMAXHASH];
struct quicklist *terrains;
struct quicklist *dirData;

#if MAXDIRECTIONS>5
const keyword_t directions[MAXDIRECTIONS] = { K_NORTH, K_SOUTH, K_EAST, K_WEST, K_MIR, K_YDD };
#else
const keyword_t directions[MAXDIRECTIONS] = { K_NORTH, K_SOUTH, K_EAST, K_WEST };
#endif
/*
const keyword_t directions[MAXDIRECTIONS] = { K_NORTH, K_NORTHEAST, K_NORTHWEST, K_SOUTH, K_SOUTHEAST, K_SOUTHWEST };
*/

region * create_region(unsigned int uid, int x, int y, const terrain * t)
{
    region * r = 0;
    region **bucket = 0;

    while (!uid || r) {
        uid = (unsigned int)genrand_int31();
        r = get_region(uid);
    };

    assert(t);
    r = (region *)malloc(sizeof(region));
    if (r) {
        memset(r, 0, sizeof(region));

        bucket = &regionhash[uid & RHASHMASK];
        r->nexthash_ = *bucket;
        *bucket = r;

        r->uid = uid;
        r->x = x;
        r->y = y;
        r->terrain = t;

        ql_push(&regions, r);
    }
    return r;
}

void free_region(region *r) {
    region **bucket;

    bucket = &regionhash[r->uid & RHASHMASK];
    while (*bucket!=r) {
        bucket = &(*bucket)->nexthash_;
    }
    *bucket = r->nexthash_;
    free(r->name_);
    while (r->units) {
        unit *u = r->units;
        r->units = u->next;
        free_unit(u);
    }
    ql_foreach(r->ships, (ql_cb)free_ship);
    ql_free(r->ships);
    r->ships = 0;
    ql_foreach(r->buildings, (ql_cb)free_building);
    ql_free(r->buildings);
    r->buildings = 0;
    free(r);
}

region * get_region(unsigned int uid)
{
    region *r = regionhash[uid & RHASHMASK];
    while (r && r->uid!=uid) {
        r = r->nexthash_;
    }
    return r;
}

region *findregion(int x, int y)
{
    ql_iter rli;

    for (rli = qli_init(&regions); qli_more(rli);) {
        region *r = (region *)qli_next(&rli);
        if (r->x == x && r->y == y)
            return r;
    }

    return 0;
}

const char * region_getname(const struct region *r)
{
    return r->name_;
}

void region_setname(struct region *r, const char *name)
{
    if (name) {
        r->name_ = (char *)realloc(r->name_, strlen(name)+1);
        strcpy(r->name_, name);
    } else {
        free(r->name_);
        r->name_ = 0;
    }
}

static void reorder_unit(unit *u, unit **ur, unit **ui) {
    assert(ui || !"no insert position given");
    assert (!u->region || ur || !"unit must be removed");
    assert (!ur || *ur==u || !"removal position does not match unit");
    if (ur!=ui && ui!=&u->next) { /* else: nothing to do */
        if (ur) {
            *ur = u->next;
        }
        u->next = *ui;
        *ui = u;
    }
}

void region_addunit(struct region *r, struct unit *u, struct unit **hint)
{
    unit **up = &r->units;
    unit **ui = up, **ur = 0;
    ql_iter bli = qli_init(&r->buildings);
    ql_iter sli = qli_init(&r->ships);
    assert(u);
    assert(!u->region || u->region==r);

    if (hint) {
        u->region = r;
        u->next = *hint;
        *hint = u;
        return;
    }
    while (*up) {
        unit *x = *up;
        if (x==u) ur = up;
        if (u->stack) {
            if (x->stack==u->stack || x==u->stack) {
                ui = &x->next;
            }
        } else if (u->building) {
            if (!x->building) {
                ui = up;
                break;
            }
            else if (x->building==u->building) {
                ui = &x->next;
            }
            else {
                struct building *b = 0;
                while (qli_more(bli)) {
                    b = (struct building *)qli_get(bli);
                    if (b==u->building) {
                        ui = up;
                        break;
                    } else if (b==x->building) {
                        break;
                    }
                    qli_next(&bli);
                }
                if (b==u->building) {
                    break;
                }
            }
        } else if (u->ship) {
            if (!x->ship) {
                ui = up;
                break;
            }
            else if (x->ship==u->ship) {
                ui = &x->next;
            }
            else {
                struct ship *s = 0;
                while (qli_more(sli)) {
                    s = (struct ship *)qli_get(sli);
                    if (s==u->ship) {
                        ui = up;
                        break;
                    } else if (s==x->ship) {
                        break;
                    }
                    qli_next(&sli);
                }
                if (s==u->ship) {
                    break;
                }
            }
        } else if (!x->next) {
            ui = &x->next;
            break;
        }
        up=&x->next;
    }
    if (u->region) {
        while (!ur && *up) {
            unit *x = *up;
            if (x==u) ur = up;
            up = &x->next;
        }
    }
    reorder_unit(u, ur, ui);
    u->region = r;
}

bool region_rmunit(struct region *r, struct unit *u, struct unit **hint)
{
    unit **up = hint ? hint : &r->units;
    while (*up) {
        unit *x = *up;
        if (x==u) {
            *up = x->next;
            u->region = 0;
            return true;
        }
        up = &x->next;
    }
    return false;

}

bool region_isocean(const struct region *r)
{
    return r->terrain == get_terrain(T_OCEAN);
}

struct terrain *create_terrain(const char * name)
{
    terrain * t = (terrain *)calloc(1, sizeof(terrain));
    ql_push(&terrains, t);
    t->name = _strdup(name);
    return t;
}

void free_terrain(terrain *t)
{
    free(t->name);
    free(t);
}

const char *terrainnames[NUMTERRAINS] = {
    "ocean", "plain", "mountain", "forest", "swamp"
};

struct terrain *get_terrain(terrain_t t)
{
    assert(t<NUMTERRAINS && t>=0);
    return get_terrain_by_name(terrainnames[t]);
}

struct terrain *get_terrain_by_name(const char *name)
{
    ql_iter qli;
    for (qli=qli_init(&terrains);qli_more(qli);) {
        terrain *t = (terrain *)qli_next(&qli);
        if (strcmp(name, t->name)==0) {
            return t;
        }
    }
    return 0;
}

void free_regions(void) {
    ql_foreach(regions, (ql_cb)free_region);
    ql_free(regions);
    regions = 0;
}

void free_terrains(void) {
    ql_foreach(terrains, (ql_cb)free_terrain);
    ql_free(terrains);
    terrains = 0;
}

region *movewhere(region * r)
{
    int dir = -1;
    keyword_t kwd = (keyword_t)getkeyword();

    switch (kwd) {
    case K_NORTH:
        dir = 0;
        break;

    case K_SOUTH:
        dir = 1;
        break;

    case K_EAST:
        dir = 2;
        break;

    case K_WEST:
        dir = 3;
        break;
#if MAXDIRECTIONS > 5
    case K_MIR:
        dir = 4;
        break;

    case K_YDD:
        dir = 5;
        break;
#endif
    default:
        dir = -1;
    }

    if (dir>=0) {
        return r->connect[dir];
    }
    return 0;
}

int transform_kwd(int *x, int *y, keyword_t kwd)
{
    ql_iter qli;
    int found = 0;
    dirStruct *d;
    assert(x || !"invalid reference to X coordinate");
    assert(y || !"invalid reference to Y coordinate");
    for (qli=qli_init(&dirData);qli_more(qli) && !found;) {
        d = (dirStruct *)qli_next(&qli);
        if (kwd == d->token) {
            *x+=d->xMod;
            *y+=d->yMod;
            found = 1;
        }
    }
    if (!found) {
        return EINVAL;
    }
    if (config.width && config.height) {
        if (*x<0) *x+=config.width;
        if (*y<0) *y+=config.height;
        if (*x>=config.width) *x-=config.width;
        if (*y>=config.height) *y-=config.height;
    }
    return 0;
}

int transform(int *x, int *y, int direction)
{
    keyword_t kwd;
    assert(direction<=MAXDIRECTIONS);
    kwd = (direction<MAXDIRECTIONS) ? directions[direction] : MAXKEYWORDS;
    return transform_kwd(x, y, kwd);
}

void add_direction(keyword_t t, int x, int y, const char *sName, const char *lName)
{
    dirStruct *d = (dirStruct *)calloc(1, sizeof(dirStruct));
    d->token = t;
    d->xMod = x;
    d->yMod = y;
    d->shortName = _strdup(sName);
    d->longName = _strdup(lName);
    ql_push(&dirData, d);
}

void read_directions()
{
#if MAXDIRECTIONS>5
    add_direction(K_NORTH, 0, -1, "n", "north");
    add_direction(K_SOUTH, 0, 1, "s", "south");
    add_direction(K_WEST, -1, 0, "w", "west");
    add_direction(K_EAST, 1, 0, "e", "east");
    add_direction(K_MIR, -1, -1, "m", "mir");
    add_direction(K_YDD, 1, 1, "y", "ydd");
#else
    add_direction(K_NORTH, 0, -1, "n", "north");
    add_direction(K_SOUTH, 0, 1, "s", "south");
    add_direction(K_WEST, -1, 0, "w", "west");
    add_direction(K_EAST, 1, 0, "e", "east");
#endif
/*
    add_direction(K_NORTH, 0, -1, "n", "north");
    add_direction(K_NORTHWEST, -1, -1, "nw", "northwest");
    add_direction(K_NORTHEAST, 1, -1, "ne", "northeast");
    add_direction(K_SOUTH, 0, 1, "s", "south");
    add_direction(K_SOUTHWEST, -1, 1, "sw", "southwest");
    add_direction(K_SOUTHEAST, 1, 1, "se", "southeast");
*/
}
