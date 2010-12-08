#include <cstdlib>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <algorithm>
#include <deque>

#include "image.h"
#include "gui.h"
#include "workers.h"

double vals[700][700][50];

FILE* logfile = stdout;
FILE* output = fopen("output.plot", "w");

enum PType {NOTHING = 0, LINE, CURVE, CORNER, DOT, MESS};
struct Profile
{
    PType type;
    int strength;
    int A, B, C;
    Profile(PType en = NOTHING, int str = 0) : type(en), strength(str) {}
};

Profile getProfile(double* tab, int len)
{
    /* szukamy dwóch minimów */
    double maxv = 0;
    const int SDiff = 10; //mała różnica w gradiencie
    std::deque< std::pair<int,int> > minims;
    int s1 = -1, s2 = -1; //poczatek, dlugosc
    for (int i=0; i<=len; i++)
    {
        maxv = std::max(maxv, tab[i]);
        if (i == len || tab[i] > 2*SDiff || tab[i] > SDiff && (s1==-1||tab[i] > 5+tab[s1])) 
        {
            if (s1 != -1) minims.push_back(std::make_pair(s1,s2));
            s1 = s2 = -1;
        }
        else
        {
            if (s1 == -1) s1 = i, s2 = 1;
            else s2 ++;
        }
    }
    
    double sumbig = 0; int ind = -1;
    for (int i=0; i<len; i++)
    {
        if (i == minims[ind+1].first)
            ind ++;
        else if (ind == -1 || i > minims[ind].first+minims[ind].second) 
            sumbig += tab[i];
        else 
            ;
    }
    
    if (minims.empty()) return Profile(NOTHING, -1);
    
    if (minims[0].first == 0 && minims.back().first+minims.back().second == len)
        minims.back().second += minims[0].second,
        minims.pop_front();
    
    int angle = -1; //rozmiar kąta
    
    Profile ret(NOTHING, angle);
    if (minims.size() > 2) 
    {
        ret.strength = minims.size();
        ret.A = minims[0].first, ret.B = minims[1].first, ret.C = minims[2].first;
        return ret;
    }
    
    if (minims.size() == 1)
    {
        angle = abs(minims[0].second - len/2);
    }
    else
    {
        int m1 = minims[0].first+minims[0].second/2, m2 = minims[1].first+minims[1].second/2;
        angle = std::min( abs(m1-m2), len-abs(m1-m2) );
        angle = abs(angle-len/2);
    }
    
    angle = angle * 360 / len;
        
    ret.A = minims.size();
    ret.B = minims.size() > 0 ? minims[0].first : -1;
    ret.C = minims.size() > 1 ? minims[1].first : -1;
    
    if (maxv > 40 && angle <= 20) ret.type = LINE;
    else if (maxv > 40 && angle > 20 && angle <= 50) ret.type = CURVE;
    else if (maxv > 50 && angle > 50 && angle <= 90) ret.type = CORNER;
    else ;
    return ret;
}

class DifferenceJob : public AsyncJob
{
    double angle;
public:
    const Image *src;
    Image *dst;
    int radius;
    int steps;
    
    int iter;
    
    virtual ~DifferenceJob();
    virtual void run();
    
    // double vals[10][10][20];
    void computeValues();
};


DifferenceJob::~DifferenceJob()
{
    /* empty */
}

void DifferenceJob::computeValues()
{
    assert(steps < 50);
    
    int border_x = ceil(radius),
        border_y = ceil(radius);
        
    static double weight[50];
    
    const int prec = 4;
    int w = src->getWidth(), h = src->getHeight();
    for(int y = border_y; y < h - border_y; y++) {
        for(int x = border_x; x < w - border_x; x++)
        {
            for (int i=0; i<steps; i++) vals[y][x][i] = 0.0, weight[i] = 0.0;
            
            for (int dx=-prec*radius; dx<=prec*radius; dx++)
                for (int dy=-prec*radius; dy<=prec*radius; dy++) if (dx*dx+dy*dy <= (prec*prec+1)*radius)//każdy piksel dzielimy na 4x4
                {
                    double tmpangle = atan2(dx, dy) + M_PI; //teraz 0 .. 2pi
                    int ind = tmpangle/(double)2/M_PI*(double)steps;
                    if (ind >= steps) ind = 0;
                    if (ind<0) ind = steps-1;
                    // if (x%10==0&&y%10==0) fprintf(logfile, "%d, %d -> %d\n", dx, dy, ind);
                    vals[y][x][ind] += (*src)[y+dy/prec][x+dx/prec];
                    weight[ind] += 1.0;
                }
            
            for (int i=0; i<steps; i++) 
            {
                vals[y][x][i] /= weight[i];
                vals[y][x][i] -= (double)(*src)[y][x];
                vals[y][x][i] = std::min(255.0, std::max(1.5, round(fabs(vals[y][x][i]*2) )));
            }
            
            /* for plotting */
            double maxx = 0.0;
            for (int i=0; i<steps; i++) maxx = std::max(maxx, vals[y][x][i]);
            if (maxx > 10.0)
            {
                Profile P = getProfile(vals[y][x], steps);
                fprintf(output, "%d %d %d %d %d %d %d ", y, x, P.type, P.strength, P.A, P.B, P.C);
                for (int i=0; i<steps || !fprintf(output, "\n"); i++) fprintf(output, "%d ", (int)vals[y][x][i]);
            }
            /* end for plotting */
        }
        if (y%10 == 0) fprintf(logfile, "ended %d\n", y);
    }
}

void DifferenceJob::run()
{
    int border_x = ceil(radius),
        border_y = ceil(radius);

    dst->fill(0);

    int w = src->getWidth(), h = src->getHeight();
    
    for(int y = border_y; y < h - border_y; y++)
        for(int x = border_x; x < w - border_x; x++)
        {
            // (*dst)[y][x] = (char) (vals[y][x][iter]>100.0 ? 255 : 2);
            Profile P = getProfile(vals[y][x], steps);
            (*dst)[y][x] = P.type == LINE || P.type == CURVE ? 255 : 2; //P.type == LINE || P.type == CURVE ? 50 : (P.type == CORNER ? 255 : 2);
            // if (x%20==0 && y%20==0 && vals[y][x][iter] >= 1.0 ) fprintf(logfile, "%d %d -> %lf\n", y, x, vals[y][x][iter]);
        }
}

int main(int argc, char *argv[])
{
    
    // ImageBase<double> foo(1,1);
    
    gui_gtk_init(&argc, &argv);
    
    if(argc != 4) {
        fprintf(stderr, "usage: poi [input filename] [radius] [steps]\n");
        return 1;
    }

    Image src = Image::readPGM(argv[1]);
    
    double radius = strtod(argv[2], NULL);
    int steps = strtol(argv[3], 0, NULL);

    DisplaySlot orig_ds, diff_ds;

    orig_ds.rename("original image");
    orig_ds.recaption("");
    
    diff_ds.rename("difference image");
    diff_ds.recaption("");

    orig_ds.update(src);

    Image dst(src.getWidth(), src.getHeight());
    DifferenceJob job;
    job.src = &src;
    job.dst = &dst;
    job.steps = steps;
    job.radius = radius;
    
    job.computeValues();
    
    for(int i=0; ; i = (i==steps-1 ? 0 : i+1))
    {
        double angle = 2.0*M_PI / (double)steps;
        
        job.iter = i;
        job.run();

        diff_ds.update(dst);
        diff_ds.recaption("rotation: %d degrees, radius: %.1lf, iter: %d",
                          (int)round(angle/M_PI*180.0), radius, i);

        usleep(1000000 / steps);
    }

    return 0;
}

/* 

Me :: 22:03:30 
image.C.: (text+0x1cf): undefined reference to `ImageBase<unsigned char>::~ImageBase()'
(...)
Me :: 22:04:20 
dobra, to ja chyba nie iwem jak się używa słowa virtual
(...)
wixor :: 22:05:07 
w template-ach wszystkie metody musza byc w klasie
(...)
wixor :: 22:05:17 
bo to jest wspanialy c++
wixor :: 22:05:26 
ewentualnie mozna tez inaczej, ale nie chcesz wiedziec, jak
(...)
wixor :: 22:15:04 
poi.C.: (text+0x669): undefined reference to `ImageBase<unsigned char>::ImageBase(int, int)'
(...)
wixor :: 22:15:55 
template-y to są makra
(...)
Me :: 22:19:15 
dobra, działa
(...)
Me :: 22:21:41 
ciekawe, czy są na świecie ludzie którzy znają cały c++
wixor :: 22:21:56 
pewnie kilku jest
wixor :: 22:22:01 
ale nie chcialbym ich poznac

*/



