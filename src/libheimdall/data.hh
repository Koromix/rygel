// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"

struct Element {
    enum class Type {
        Event,
        Measure,
        Period
    };

    const char *concept;
    double time;
    int source_id;

    Type type;
    union {
        struct { double value; double min; double max; } measure;
        struct { double duration; } period;
    } u;
};

struct Entity {
    const char *id;
    HeapArray<Element> elements;
};

struct SourceInfo {
    const char *name;
    const char *default_path;
};

struct Concept {
    const char *name;
    const char *title;
    const char *path;

    HASH_TABLE_HANDLER(Concept, name);
};

struct EntitySet {
    HashMap<int, SourceInfo> sources;
    HeapArray<Entity> entities;

    HashSet<const char *> paths_set;
    HeapArray<const char *> paths;
    HashTable<const char *, Concept> concepts_map;

    // TODO: Use a string pool to use less memory and for faster string tests
    LinkedAllocator str_alloc;
};
