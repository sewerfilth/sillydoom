# 04 — 64-bit Porting Guide

linuxdoom-1.10 was written for 32-bit i386. Porting to 64-bit ARM64 (macOS
Apple Silicon, Nintendo Switch) required fixes in several categories.

## Pointer Size Assumptions

The original code assumes `sizeof(void*) == 4` in many places.

### WAD Overlay Structs

Structs used to overlay raw WAD data must have fixed-size fields. The
`maptexture_t` struct had a `void **columndirectory` field that expanded
from 4 to 8 bytes on 64-bit, shifting all subsequent fields:

```c
// BROKEN on 64-bit:
typedef struct {
    char        name[8];
    boolean     masked;
    short       width, height;
    void      **columndirectory;  // 4 bytes on 32-bit, 8 on 64-bit!
    short       patchcount;
    mappatch_t  patches[1];
} maptexture_t;

// FIXED:
typedef struct {
    char        name[8];
    int         masked;
    short       width, height;
    int         columndirectory;  // always 4 bytes (field is obsolete)
    short       patchcount;
    mappatch_t  patches[1];
} maptexture_t;
```

**File:** `r_data.c`

### Zone Allocator

`memblock_t.size` and `memzone_t.size` were `int`, causing pointer arithmetic
overflow when the zone base address exceeds 4GB:

```c
// FIXED: int → intptr_t
typedef struct memblock_s {
    intptr_t        size;
    void**          user;
    int             tag;
    int             id;
    struct memblock_s* next;
    struct memblock_s* prev;
} memblock_t;
```

**Files:** `z_zone.h`, `z_zone.c`

### Pointer-to-int Casts

All `(int)pointer` casts changed to `(intptr_t)`:

| File | Pattern | Fix |
|------|---------|-----|
| `r_data.c` | `(int)colormaps + 255` | `(intptr_t)colormaps + 255` |
| `r_draw.c` | `(int)translationtables` | `(intptr_t)translationtables` |
| `p_saveg.c` | `(int)save_p & 3` | `(intptr_t)save_p & 3` |
| `p_saveg.c` | `(int)mobj->state` | `(intptr_t)mobj->state` |
| `p_saveg.c` | `(int)ceiling->sector` | `(intptr_t)ceiling->sector` |
| `d_net.c` | `(int)&(doomdata_t*)0->cmds` | `(intptr_t)&...` |
| `p_setup.c` | `total*4` (pointer array) | `total*sizeof(line_t*)` |

### Pointer Array Allocations

The original `numtextures*4` for pointer arrays only allocates half the
needed memory on 64-bit:

```c
// BROKEN: assumes 4-byte pointers
textures = Z_Malloc(numtextures*4, PU_STATIC, 0);

// FIXED:
textures = Z_Malloc(numtextures*sizeof(texture_t*), PU_STATIC, 0);
```

**File:** `r_data.c`

## Platform Compatibility

### `values.h`

Replaced with `<limits.h>` + `<float.h>` + `<stdint.h>` in `doomtype.h`.

### `alloca()`

All `alloca()` calls replaced with `malloc()` to prevent stack overflow
on large texture arrays.

### `strupr()`

Renamed to `doom_strupr()` to avoid conflict with devkitPro newlib.

### `NUMSPRITES` Array Terminator

`sprnames[NUMSPRITES]` has no NULL terminator. The count loop
`while (*check != NULL)` read past the array. Fixed by using
`NUMSPRITES` enum value directly.

**File:** `r_things.c`

## Renderer Limits

Vanilla DOOM limits are tight for 64-bit due to larger struct sizes.
Raised for stability:

| Limit | Original | sillydoom | File |
|-------|----------|-----------|------|
| `MAXDRAWSEGS` | 256 | 2048 | `r_defs.h` |
| `MAXVISSPRITES` | 128 | 1024 | `r_things.h` |
| `MAXVISPLANES` | 128 | 1024 | `r_plane.c` |
| `MAXOPENINGS` | SCREENWIDTH*64 | SCREENWIDTH*256 | `r_plane.c` |

## Demo Compatibility

DOOM v1.10 engine accepts demos from v1.9 (version byte 109) in
addition to v1.10 (version byte 110). The demo format is identical.

**File:** `g_game.c`
