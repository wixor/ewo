#include <cstdlib>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <algorithm>

#include "image.h"
#include "gui.h"
#include "workers.h"

struct rect
{
    double x1,y1,x2,y2;

    inline rect() { }
    inline rect(double x1, double y1, double x2, double y2) : x1(x1), y1(y1), x2(x2), y2(y2) { }

    static rect unitAt(double x1, double y1) {
        return rect(x1,y1,x1+1,y1+1);
    }

    inline rect intersect(const rect &r) const {
        return rect(std::max(x1, r.x1), std::max(y1, r.y1),
                    std::min(x2, r.x2), std::min(y2, r.y2));
    }
    inline double area() const {
        return std::abs(x2-x1) * std::abs(y2-y1);
    }
};

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
    double dx, dy;
    double scale;

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
            double val = 0.0
                + avgField(0.5+x-scale/2.0, 0.5+x+scale/2.0, 0.5+y-scale/2.0, 0.5+y+scale/2.0) //per coord
                - avgField(0.5+x+dx-scale/2.0, 0.5+x+dx+scale/2.0, 0.5+y+dy-scale/2.0, 0.5+y+dy+scale/2.0); //per coord
            
            int valint = (int)round(fabs(val));
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
    
    Image dst(src.getWidth(), src.getHeight());
    DifferenceJob job;
    job.src = &src;
    job.dst = &dst;
    job.scale = scale;
    
    job.makeSumPref();
    
    for(int i=0; ; i = (i==steps-1 ? 0 : i+1))
    {
        double angle = 2.0*M_PI / (double)steps * (double)i;

        job.dx = radius*cos(angle);
        job.dy = radius*sin(angle);
        job.run();

        diff_ds.update(dst);
        diff_ds.recaption("rotation: %d degrees, radius: %.1lf, dx: %.1lf, dy: %.1lf",
                          (int)round(angle/M_PI*180.0), radius, job.dx, job.dy);

        usleep(1000000 / steps);
    }

    return 0;
}



