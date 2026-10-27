/*
 * owlight_c.h — the C interface to the Owlight simulation core.
 *
 * This is what Unreal (or anything else) calls. No C++ types cross this boundary: handles are
 * opaque pointers, ids are NUL-terminated UTF-8 strings, and times are hours as doubles.
 *
 * Lifetimes
 *   - owl_catalog_builtin() is owned by the library; never free it.
 *   - A catalog loaded with owl_catalog_load() must outlive every slime created from it, and is
 *     released with owl_catalog_free().
 *   - A slime is released with owl_slime_free().
 *   - Every `const char*` returned for a slime stays valid until the next call that changes that
 *     slime (feed, tick, name, pick up, load). Copy it if you need it longer. Catalog ids stay
 *     valid as long as the catalog does.
 *
 * Errors
 *   - Functions that can fail return 0 on success and a negative code on failure (see the
 *     per-function notes); where a message helps, it is written into the caller's buffer.
 *
 * Everything the player may see comes from owl_slime_type(), owl_slime_sign() and the journal.
 * owl_slime_value() exposes the hidden numbers for tuning tools only — never show them in game.
 */

#ifndef OWLIGHT_C_H
#define OWLIGHT_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Define OWLIGHT_SHARED when the core is built or used as a shared library (Unreal's modular
   editor builds do this), and OWLIGHT_BUILD inside the library itself. */
#if defined(OWLIGHT_SHARED)
#  if defined(_WIN32)
#    ifdef OWLIGHT_BUILD
#      define OWL_API __declspec(dllexport)
#    else
#      define OWL_API __declspec(dllimport)
#    endif
#  else
#    define OWL_API __attribute__((visibility("default")))
#  endif
#else
#  define OWL_API
#endif

typedef struct owl_catalog owl_catalog;
typedef struct owl_slime owl_slime;

/* Which catalog list a call refers to. */
enum owl_kind { OWL_FOOD = 0, OWL_HABITAT = 1, OWL_JOB = 2, OWL_TYPE = 3 };

/* The hidden properties, in the order the design doc lists them. */
enum owl_property {
    OWL_DENSITY = 0, OWL_HEAT, OWL_ACIDITY, OWL_CLARITY, OWL_APPETITE,
    OWL_BOLDNESS, OWL_MEMORY, OWL_AFFINITY, OWL_LIGHT, OWL_BOND,
    OWL_PROPERTY_COUNT
};

OWL_API const char* owl_version(void);

/* --- Catalog ----------------------------------------------------------------------------- */

/* The shipped catalog (the data JSON embedded at build time). Never free. */
OWL_API const owl_catalog* owl_catalog_builtin(void);

/* Load a catalog from a directory holding foods.json, habitats.json, jobs.json, types.json.
   Returns NULL on failure and writes the reason ("types.json:42: ...") into err if given. */
OWL_API owl_catalog* owl_catalog_load(const char* dir, char* err, size_t err_cap);
OWL_API void owl_catalog_free(owl_catalog* catalog);

OWL_API int         owl_catalog_count(const owl_catalog* catalog, enum owl_kind kind);
OWL_API const char* owl_catalog_id(const owl_catalog* catalog, enum owl_kind kind, int index); /* NULL if out of range */
OWL_API int         owl_catalog_has(const owl_catalog* catalog, enum owl_kind kind, const char* id);
OWL_API int         owl_catalog_type_is_rare(const owl_catalog* catalog, const char* type_id);     /* -1 if unknown */

/* --- Slime lifetime ------------------------------------------------------------------------- */

/* A fresh slime. seed 0 is the resting slime; other seeds add small deterministic variance. */
OWL_API owl_slime* owl_slime_new(const owl_catalog* catalog, const char* name, unsigned seed);

/* Resume from a save written by owl_slime_save(). NULL on failure; reason into err if given. */
OWL_API owl_slime* owl_slime_load(const owl_catalog* catalog, const char* json, char* err, size_t err_cap);
OWL_API void       owl_slime_free(owl_slime* slime);

/* Write the save as JSON into buf. Returns the full length needed (not counting the NUL). If
   buf_cap is too small nothing is written beyond a terminating NUL at buf[0]; call again with a
   bigger buffer. Pass buf = NULL, buf_cap = 0 to ask for the size. */
OWL_API size_t owl_slime_save(const owl_slime* slime, char* buf, size_t buf_cap);

/* --- Actions ------------------------------------------------------------------------------- */

/* 0 ok, -1 unknown food. */
OWL_API int  owl_slime_feed(owl_slime* slime, const char* food_id);
OWL_API void owl_slime_pick_up(owl_slime* slime);
OWL_API void owl_slime_give_name(owl_slime* slime, const char* name);

/* Advance the slime by `hours` of real time in the given surroundings. job_id may be NULL
   (nothing to do). Safe for any positive hours, large or tiny. 0 ok, -1 unknown habitat,
   -2 unknown job, -3 hours not positive. */
OWL_API int owl_slime_tick(owl_slime* slime, double hours, const char* habitat_id, const char* job_id,
                           int near_player, int is_daytime);

/* --- What the player can see ------------------------------------------------------------- */

OWL_API const char* owl_slime_name(const owl_slime* slime);
OWL_API const char* owl_slime_type(const owl_slime* slime);     /* e.g. "plain", "soot" */
OWL_API int         owl_slime_locked(const owl_slime* slime);
OWL_API double      owl_slime_age_hours(const owl_slime* slime);

/* Visible signs: short lines like "sits heavy where it rests". Only non-empty signs are listed.
   owl_slime_sign_count() refreshes the list; owl_slime_sign(i) reads from it. */
OWL_API int         owl_slime_sign_count(owl_slime* slime);
OWL_API const char* owl_slime_sign(const owl_slime* slime, int index);               /* NULL if out of range */
OWL_API int         owl_slime_sign_property(const owl_slime* slime, int index);       /* owl_property, or -1 */

/* The journal, oldest first. kind is one of: fed moved assigned sign stirring became reverted
   locked note. Any out-pointer may be NULL. Returns 0 ok, -1 out of range. */
OWL_API int owl_slime_journal_count(const owl_slime* slime);
OWL_API int owl_slime_journal_entry(const owl_slime* slime, int index, double* at_hours,
                                    const char** kind, const char** text);

/* --- Hidden (tuning tools only) ---------------------------------------------------------- */

OWL_API double      owl_slime_value(const owl_slime* slime, enum owl_property property);   /* 0..1, NaN if bad */
OWL_API const char* owl_slime_affinity_material(const owl_slime* slime);                   /* "none", "metal", ... */

#ifdef __cplusplus
}
#endif

#endif /* OWLIGHT_C_H */
