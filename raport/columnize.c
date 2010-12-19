#include <stdio.h>

int main(int argc, char *argv[])
{
    int n = argc-1;

    if(n < 2) {
        fprintf(stderr, "USAGE: average [file1] [file2] [more files...]\n");
        return 1;
    }

    FILE *fs[n];
    for(int i=1; i<=n; i++) {
        fs[i-1] = fopen(argv[i], "r");
        if(fs[i-1] == NULL) {
            fprintf(stderr, "failed to open file '%s'\n", argv[i]);
            return 2;
        }
    }

    for(;;)
    {
        float v;
        if(fscanf(fs[0], "%f", &v) != 1) break;
        printf("%10f ", v);
        for(int i=1; i<n; i++) {
            if(fscanf(fs[i], "%f", &v) != 1) {
                fprintf(stderr, "unmatched lines\n");
                return 3;
            }
            printf("%12.6f ", v);
        }
        putchar('\n');
    }
    return 0;
}
