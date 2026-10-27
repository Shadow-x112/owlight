/* Compiled as plain C: proves owlight_c.h is a real C header and the first day works through
 * it. Mirrors scenarios/first_day.json. Exit code is the number of failures. */

#include "owlight/owlight_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { ++failures; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void hours(owl_slime* s, int n, const char* habitat, const char* job, int near) {
    /* Day is 06:00-20:00; tick one hour at a time so day/night flips land on the hour. */
    int i;
    for (i = 0; i < n; ++i) {
        int h = (int)owl_slime_age_hours(s) % 24;
        int day = h >= 6 && h < 20;
        CHECK(owl_slime_tick(s, 1.0, habitat, job, near && day, day) == 0);
    }
}

int main(void) {
    const owl_catalog* cat = owl_catalog_builtin();
    owl_slime* s;
    char* save;
    size_t need;
    int n, i, found;

    CHECK(cat != NULL);
    CHECK(owl_catalog_count(cat, OWL_FOOD) > 10);
    CHECK(owl_catalog_has(cat, OWL_FOOD, "charcoal"));
    CHECK(!owl_catalog_has(cat, OWL_FOOD, "charcol"));
    CHECK(owl_catalog_has(cat, OWL_TYPE, "plain"));
    CHECK(owl_catalog_type_is_rare(cat, "pearl") == 1);
    CHECK(owl_catalog_type_is_rare(cat, "soot") == 0);
    CHECK(owl_catalog_type_is_rare(cat, "nope") == -1);

    s = owl_slime_new(cat, "slime", 0);
    CHECK(s != NULL);
    CHECK(strcmp(owl_slime_type(s), "plain") == 0);

    /* Day 1 */
    hours(s, 8, "pen", NULL, 1);
    CHECK(owl_slime_feed(s, "charcoal") == 0);
    CHECK(owl_slime_feed(s, "charcol") == -1);
    hours(s, 5, "pen", NULL, 1);
    owl_slime_pick_up(s);
    CHECK(owl_slime_feed(s, "charcoal") == 0);
    hours(s, 5, "pen", NULL, 1);
    CHECK(owl_slime_feed(s, "charcoal") == 0);
    CHECK(strcmp(owl_slime_type(s), "plain") == 0);
    n = owl_slime_sign_count(s);
    found = 0;
    for (i = 0; i < n; ++i) if (strstr(owl_slime_sign(s, i), "cloudy")) found = 1;
    CHECK(found);
    hours(s, 3, "pen", NULL, 1);
    owl_slime_give_name(s, "Cinder");
    CHECK(strcmp(owl_slime_name(s), "Cinder") == 0);

    /* Night, then the morning feeding */
    hours(s, 11, "pen", NULL, 0);
    CHECK(strcmp(owl_slime_type(s), "plain") == 0);
    CHECK(owl_slime_feed(s, "charcoal") == 0);
    CHECK(strcmp(owl_slime_type(s), "soot") == 0);
    CHECK(owl_slime_locked(s) == 0);

    /* Save, free, load, continue. */
    need = owl_slime_save(s, NULL, 0);
    CHECK(need > 100);
    save = (char*)malloc(need + 1);
    CHECK(owl_slime_save(s, save, need + 1) == need);
    CHECK(strlen(save) == need);
    {
        char err[256];
        owl_slime* t = owl_slime_load(cat, save, err, sizeof err);
        CHECK(t != NULL);
        if (t) {
            CHECK(strcmp(owl_slime_name(t), "Cinder") == 0);
            CHECK(strcmp(owl_slime_type(t), "soot") == 0);
            CHECK(owl_slime_age_hours(t) == owl_slime_age_hours(s));
            hours(t, 4, "stove_corner", "scrub_stove", 1);
            hours(s, 4, "stove_corner", "scrub_stove", 1);
            CHECK(owl_slime_value(t, OWL_DENSITY) == owl_slime_value(s, OWL_DENSITY));
            CHECK(owl_slime_journal_count(t) == owl_slime_journal_count(s));
            owl_slime_free(t);
        }
        CHECK(owl_slime_load(cat, "{", err, sizeof err) == NULL);
        CHECK(strlen(err) > 0);
    }
    free(save);

    /* Journal reads back. */
    n = owl_slime_journal_count(s);
    CHECK(n > 5);
    found = 0;
    for (i = 0; i < n; ++i) {
        double at; const char* kind; const char* text;
        CHECK(owl_slime_journal_entry(s, i, &at, &kind, &text) == 0);
        if (strcmp(kind, "became") == 0 && strstr(text, "soot")) found = 1;
    }
    CHECK(found);
    CHECK(owl_slime_journal_entry(s, n, NULL, NULL, NULL) == -1);

    /* Bad ticks. */
    CHECK(owl_slime_tick(s, 1.0, "nowhere", NULL, 0, 1) == -1);
    CHECK(owl_slime_tick(s, 1.0, "pen", "nojob", 0, 1) == -2);
    CHECK(owl_slime_tick(s, 0.0, "pen", NULL, 0, 1) == -3);

    owl_slime_free(s);
    owl_slime_free(NULL);
    owl_catalog_free(NULL);

    printf("c api: %d failure(s)\n", failures);
    return failures;
}
