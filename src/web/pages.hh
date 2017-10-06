#pragma once

#include "../core/libmoya.hh"

struct Page {
    const char *const url;
    const char *const name;
};

static const Page pages[] = {
    {"/pricing", "Valorisation"},
    {"/simulation", "Simulation"},
    {"/info", "Tables de groupage"}
};

extern const ArrayRef<const char> page_index;
