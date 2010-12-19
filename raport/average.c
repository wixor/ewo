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
        float v = 0;
        if(fscanf(fs[0], "%f", &v) != 1) break;
        for(int i=1; i<n; i++) {
            float x;
            if(fscanf(fs[i], "%f", &x) != 1) {
                fprintf(stderr, "unmatched lines\n");
                return 3;
            }
            v += x;
        }
        printf("%f\n", v/n);
    }
    return 0;
}
