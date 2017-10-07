#pragma once

#include "../moya/libmoya.hh"

struct Page {
    const char *const url;
    const char *const name;
};

static const Page pages[] = {
    {"/pricing", "Valorisation"},
    {"/simulation", "Simulation"},
    {"/info", "Tables de groupage"}
};
