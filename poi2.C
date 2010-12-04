#include <cstdlib>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <algorithm>

#include "image.h"
#include "gui.h"
#include "workers.h"

double vals[700][700][50];

FILE* logfile = stdout;
FILE* output = fopen("output.plot", "w");

enum ProfileEnum {LINE, DOT, CORNER, MESS, NOTHING};
struct Profile
{
    ProfileEnum type;
    int strength;
    Profile(ProfileEnum en = NOTHING, int str = 0) : type(en), strength(str) {}
};

Profile getProfile(double* tab, int len)
{
    /* szukamy dwóch minimów */
    for (int i=0; i<len; i++)
    {
        ;
    }
    return NOTHING;
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
    
    const int prec = 6;
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
                fprintf(output, "%d %d ", y, x);
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
            (*dst)[y][x] = (char) (vals[y][x][iter]>100.0 ? 255 : 2);
            // if (x%20==0 && y%20==0 && vals[y][x][iter] >= 1.0 ) fprintf(logfile, "%d %d -> %lf\n", y, x, vals[y][x][iter]);
        }
}


int main(int argc, char *argv[])
{
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
