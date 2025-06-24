// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/base/base.hh"

namespace RG {

struct Element {
    enum class Type {
        Event,
        Measure,
        Period
    };

    const char *concept_name = nullptr;
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

struct EntitySet {
    HashMap<int, const char *> sources;
    HeapArray<Entity> entities;

    LinkedAllocator str_alloc;
};

struct Concept {
    const char *name = nullptr;
    const char *path = nullptr;

    RG_HASHTABLE_HANDLER(Concept, name);
};

struct ConceptSet {
    const char *name;

    HashSet<const char *> paths_set;
    HeapArray<const char *> paths;
    HashTable<const char *, Concept> concepts_map;

    LinkedAllocator str_alloc;
};

}
