#ifndef _MAP_H
#define _MAP_H

struct map_entry {
    const char *key;
    void *value;
    struct map_entry *next;
};

struct map {
    unsigned size, tablesize;
    unsigned grow_at, shrink_at;
    struct map_entry **table;
    int (*cmpfn) (const char *key1, const char *key2);
};

extern unsigned strhash(const char *s);

extern struct map * new_map(int (*cmpfn) (const char *, const char *));

extern void free_map(struct map *map);

extern void *map_get(struct map *map, const char *key);

extern void map_put(struct map *map, const char *key, void *value);

extern int nocmp(const char *key1, const char *key2);

extern int cmp(const char *key1, const char *key2);

#endif