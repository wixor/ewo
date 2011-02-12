#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const char foo[] = "result-";

struct TEST 
{
    vector<float> vres;
    char name[256];
    float avg;
};

bool operator<(const TEST &t1, const TEST &t2) 
{
    // return (t1.avg = avg1) < (t2.avg = avg2);
    return (t1.avg) < (t2.avg);
}

char tmp[256];

int main()
{
    vector<TEST> vtest;
    
    FILE *outp = fopen("graph", "w");
    for (int i=1; i<=20; i++) {
        char filename[11] = {};
        strcpy(filename, foo);
        filename[7] = 
            i == 20 ? '2' : 
            i >= 10 ? '1' :
            '0' + i;
        filename[8] = 
            i < 10 ? '\0' : 
            '0' + (i % 10);
        // printf("%s\n", filename);
        FILE *fp = fopen(filename, "r");
        float f;
        int ind = 0;
        while (fscanf(fp, "%s %f", tmp, &f) == 2) {
            int st=0;
            for(int i=1; i<strlen(tmp); i++) {
                if (tmp[i]=='/') st=i+1;
                if (tmp[i]=='.') tmp[i]='\0';
            }
            vtest.resize(max((int)vtest.size(), ind+1));
            vtest[ind].vres.push_back(f);
            strcpy(vtest[ind].name, tmp+st);
            ind ++;
        }
    }
    for (int T=0; T<vtest.size(); T++) {
        TEST &t = vtest[T];
        for (int i=0; i<t.vres.size(); i++) 
            t.avg += t.vres[i]/t.vres.size();
    }
    
    sort(vtest.rbegin(), vtest.rend());
    
    for (int i=0; i<vtest.size(); i++) {
        sort(vtest[i].vres.rbegin(), vtest[i].vres.rend());
        // for (int j=0; j<vtest[i].vres.size()/2; j++) fprintf(outp, "%d %.4f\n", i+1, -vtest[i].vres[j]);
        // fprintf(outp, "%s %.4f\n", vtest[i].name, vtest[i].avg);
        for (int j=0, pr=fprintf(outp, "%s ", vtest[i].name); j<vtest[i].vres.size()/2; j++) fprintf(outp, "%.4f ", -vtest[i].vres[j]);
        fprintf(outp, "\n");
    }
    
    
    return 0;
}