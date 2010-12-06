#include <cstdlib>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <algorithm>
#include <vector>

#include "image.h"
#include "gui.h"
#include "workers.h"

//FILE* output = fopen("output.plot", "w");

static int to255(double x) { return std::max(0, std::min(255, (int)round(x))); }

template <typename T> struct pair {
    T fi, se;
    pair(T f, T s) : fi(f), se(s) {}
    inline pair<T> operator+(const pair<T>& p) { return pair<T>(fi+p.fi, se+p.se); }
};
typedef pair<double> paird;

/* uwaga implementacyjna.
w tym miejscu zaczynają istnieć dwa oddzielne układy odniesienia do obrazka.
1. per pixel:
    pixel lewy górny ma współrzędne (0,0) i tak dalej
    odwołujemy się [y][x]
2. per coord[inates]:
    pixel w lewym górnym rogu jest prostokątem (0,0, 1,1)
    odwołujemy się [y][x] (no niech już będzie...)

zatem chcąc policzyć jakaś wlasność pixela (x,y) (lub, ogólniej: kwadratu KxK o środku w tym pixelu)
liczymy tę własność prostokąta (x+1/2-K/2,x+1/2+K/2, ...)
*/

struct RawProfile
{
    double tab[32];
    int steps;
    inline RawProfile() { steps = 32; memset(tab, 0, 32*sizeof(double)); }
    inline double& operator[](int i) { return tab[i]; }
    int interesting() /* tu określamy, w skali powiedzmy 0-255, interesującość funkcji */
    {
        double maxv = -1000, minv = 1000;
        for (int i=0; i<steps; i++) 
            maxv = std::max(maxv, tab[i]),
            minv = std::min(minv, tab[i]);
        return to255(fabs(maxv-minv));
    }
};

class DifferenceJob : public AsyncJob
{
    ImageBase<int> *prep;
    inline int sumWholeField(int x1, int x2, int y1, int y2) // per coord
    { 
        if (x1>x2) std::swap(x1,x2);
        if (y1>y2) std::swap(y1,y2);
        int ret =
            (*prep)[y2][x2]
            - (*prep)[y2][x1]
            - (*prep)[y1][x2]
            + (*prep)[y1][x1];
        return ret;
    }
    
public:
    const Image *src;
    Image *dst;
    ImageBase<RawProfile> *pointData;
    
    double dx, dy;
    double scale;

    int currstep;
    bool filled;

    DifferenceJob() { filled = false; currstep = 0; }
    virtual ~DifferenceJob();
    virtual void run();
    
    pair<double> sumField(double x1, double x2, double y1, double y2); //per coord
    inline double avgField(double x1, double x2, double y1, double y2) { //per coord
        pair<double> pd = sumField(x1,x2,y1,y2);
        return pd.se/pd.fi;
    }
        
    void makeSumPref();
};

DifferenceJob::~DifferenceJob()
{
    /* empty */
}

void DifferenceJob::run()
{
    int border_x = std::max(std::abs(dx-scale/2.0), std::abs(dx+1+scale/2.0)),
        border_y = std::max(std::abs(dy-scale/2.0), std::abs(dy+1+scale/2.0));

    dst->fill(0);

    int w = src->getWidth(), h = src->getHeight();
    
    for(int y = border_y; y < h - border_y; y++)
        for(int x = border_x; x < w - border_x; x++) //per pixel
        {
            int valint;
            if (!filled) 
            {
                double val =
                    + avgField(0.5+x-scale/2.0, 0.5+x+scale/2.0, 0.5+y-scale/2.0, 0.5+y+scale/2.0) //per coord
                    - avgField(0.5+x+dx-scale/2.0, 0.5+x+dx+scale/2.0, 0.5+y+dy-scale/2.0, 0.5+y+dy+scale/2.0); //per coord
                
                (*pointData)[y][x][currstep] = val;
                (*pointData)[y][x].steps = currstep+1;
                
                valint = (int)round(fabs(val));
            }
            else
            {
                // valint = (int)round(fabs((*pointData)[y][x][currstep]));
                valint = (*pointData)[y][x].interesting();
            }
            (*dst)[y][x] = std::max(std::min(valint, 255), 1); //per pixel
        }
}
    
void DifferenceJob::makeSumPref() //per coord
{
    int w = src->getWidth(), h = src->getHeight();
    prep = new ImageBase<int>(w+1, h+1);
    for (int j=0; j<=h; j++)
        for (int i=0; i<=w; i++)
        {
            if (!i || !j) { 
                (*prep)[j][i] = 0;
                continue;
            }
            
            (*prep)[j][i] = 
                + (*src)[j-1][i-1] //per pixel
                + (*prep)[j-1][i] //per coord
                + (*prep)[j][i-1] //per coord
                - (*prep)[j-1][i-1]; //per coord
        }
}

inline pair<double> DifferenceJob::sumField(double x1, double x2, double y1, double y2)
{
    if (x1>x2) std::swap(x1,x2);
    if (y1>y2) std::swap(y1,y2);
    double ix1 = ceil(x1), iy1 = ceil(y1), ix2 = floor(x2), iy2 = floor(y2);
    
    double ret =  
        (double)sumWholeField(ix1, ix2, iy1, iy2)
        
        + (ix1-x1)*sumWholeField(ix1-1, ix1, iy1, iy2)
        + (x2-ix2)*sumWholeField(ix2, ix2+1, iy1, iy2)
        + (iy1-y1)*sumWholeField(ix1, ix2, iy1-1, iy1)
        + (y2-iy2)*sumWholeField(ix1, ix2, iy2, iy2+1)
        
        + (ix1-x1)*(iy1-y1)*sumWholeField(ix1-1, ix1, iy1-1, iy1)
        + (x2-ix2)*(iy1-y1)*sumWholeField(ix2, ix2+1, iy1-1, iy1)
        + (ix1-x1)*(y2-iy2)*sumWholeField(ix1-1, ix1, iy2, iy2+1)
        + (x2-ix2)*(y2-iy2)*sumWholeField(ix2, ix2+1, iy2, iy2+1);
        
    return pair<double>( (x2-x1)*(y2-y1), ret );
}
    
    
int main(int argc, char *argv[])
{
    gui_gtk_init(&argc, &argv);
    
    if(argc != 5) {
        fprintf(stderr, "usage: poi [input filename] [radius] [steps] [scale]\n");
        return 1;
    }
    
    srand(time(0));

    Image src = Image::readPGM(argv[1]);
    double radius = strtod(argv[2], NULL);
    int steps = strtol(argv[3], 0, NULL);
    double scale = strtod(argv[4], NULL);

    DisplaySlot orig_ds, diff_ds;

    orig_ds.rename("original image");
    orig_ds.recaption("");
    diff_ds.rename("difference image");
    diff_ds.recaption("");
    orig_ds.update(src);
    
    int nScales = 5;
    const int scales[] = {1,3,5,7,10};
    
    int w = src.getWidth(), h = src.getHeight();
    Image dst(w, h);
    std::vector<ImageBase<RawProfile> > profiles(nScales, ImageBase<RawProfile>(w,h));

    DifferenceJob job;
    job.src = &src;
    job.dst = &dst;
    job.makeSumPref();
    
    for(int scaleidx = 0; scaleidx < nScales; scaleidx++)
    {
        printf("scale %d...\n", scales[scaleidx]);
        job.scale = scales[scaleidx];
        job.pointData = &profiles[scaleidx];
        
        for(int i=0; i<steps; i++) {
            printf("  step %d\n", i);
            double angle = 2*M_PI/steps*i;
            job.dx = radius*cos(angle);
            job.dy = radius*sin(angle);
            job.currstep = i;
            job.run();
            diff_ds.update(dst);
        }
    }
    
    dst.fill(0);
    for(int y=15; y<h-15; y++)
        for(int x=20; x<w-20; x++)
        {
            double minv = 1000000, maxv = -1000000;
            for(int i=0; i<steps; i++)
            {
                double v = 1;
                for(int s=0; s<nScales; s++)
                    v *= profiles[s][y][x][i];
                minv = std::min(minv, v);
                maxv = std::max(maxv, v);
            }            
            
            dst[y][x] = to255(pow(maxv-minv, 0.2));
        }
        
    dst.writePGM("output.pgm");
        
    
    
#if 0
    job.pointData = new ImageBase<RawProfile>(w, h);
    
    
    
    for(int i=0; ; i = (i==steps-1 ? 0 : i+1))
    {
        
        double angle = 2.0*M_PI / (double)steps * (double)i;

        job.dx = radius*cos(angle);
        job.dy = radius*sin(angle);
        job.currstep = i;
        job.run();

        diff_ds.update(dst);
        diff_ds.recaption("rotation: %d degrees, radius: %.1lf, dx: %.1lf, dy: %.1lf",
                          (int)round(angle/M_PI*180.0), radius, job.dx, job.dy);

        usleep(1000000 / steps);
        
        if (!job.filled && job.currstep == steps-1)
        {
            job.filled = true;
            for (int x=0; x<src.getWidth(); x++)
                for (int y=0; y<src.getHeight(); y++)
                {
                    RawProfile& P = (*job.pointData)[y][x];
                    if (P.interesting() < 50) continue;
                    fprintf(output, "%d %d ", x, y);
                    for (int k=0; k<steps || !fprintf(output, "\n"); k++)
                        fprintf(output, "%.2lf ", P[k]);
                }
            printf("written out!\n");
            fclose(output);
        }
    }
#endif

    return 0;
}



