/* Simpler gnu89 version of StandaloneFuzzTargetMain.c from LLVM */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput (const unsigned char *data, size_t size);
__attribute__((weak)) int LLVMFuzzerInitialize(int *argc, char ***argv);

int
main (int argc, char **argv)
{
    FILE *f = NULL;
    size_t n_read, len;
    unsigned char *buf = NULL;

    if (argc < 2) {
        return 1;
    }

    if (LLVMFuzzerInitialize) {
        LLVMFuzzerInitialize(&argc, &argv);
    }

    f = fopen (argv[1], "r");
    assert (f);
    fseek (f, 0, SEEK_END);
    len = ftell (f);
    fseek (f, 0, SEEK_SET);
    buf = (unsigned char*) malloc (len);
    n_read = fread (buf, 1, len, f);
    fclose (f);
    assert (n_read == len);
    LLVMFuzzerTestOneInput (buf, len);

    free (buf);
    printf ("Done!\n");
    return 0;
}
