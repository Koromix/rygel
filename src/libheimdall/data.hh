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

    const char *concept = nullptr;
    double time = 0.0;
    int source_id = 0;

    Type type = Type::Event;
    union {
        struct { double value; double min; double max; } measure;
        struct { double duration; } period;
    } u;
};

struct Entity {
    const char *id = nullptr;
    HeapArray<Element> elements;
};

struct SourceInfo {
    const char *name = nullptr;
    const char *default_path = nullptr;
};

struct EntitySet {
    HashMap<int, SourceInfo> sources;
    HeapArray<Entity> entities;

    LinkedAllocator str_alloc;
};

struct Concept {
    const char *name = nullptr;
    const char *title = nullptr;
    const char *path = nullptr;

    HASH_TABLE_HANDLER(Concept, name);
};

struct ConceptSet {
    const char *name;

    HashSet<const char *> paths_set;
    HeapArray<const char *> paths;
    HashTable<const char *, Concept> concepts_map;

    LinkedAllocator str_alloc;
};
