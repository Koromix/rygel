extern "C" int Trampoline0;
extern "C" int Trampoline1;
extern "C" int Trampoline8191;

static const void *TrampolineLast = &Trampoline8191;

static_assert(MaxTrampolines == 8192);
