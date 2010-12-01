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

class DifferenceJob : public AsyncJob
{
public:
    const Image *src;
    Image *dst;
    double dx, dy;

    virtual ~DifferenceJob();
    virtual void run();
};


DifferenceJob::~DifferenceJob()
{
    /* empty */
}


void DifferenceJob::run()
{
    const rect pixel = rect::unitAt(this->dx, this->dy);
    int dx = floor(this->dx),
        dy = floor(this->dy);
    double w0 = pixel.intersect(rect::unitAt(dx,  dy  )).area(),
           w1 = pixel.intersect(rect::unitAt(dx+1,dy  )).area(),
           w2 = pixel.intersect(rect::unitAt(dx,  dy+1)).area(),
           w3 = pixel.intersect(rect::unitAt(dx+1,dy+1)).area();

    int border_x = std::max(std::abs(dx), std::abs(dx+1)),
        border_y = std::max(std::abs(dy), std::abs(dy+1));


    assert(w0+w1+w2+w3 >= 0.99 && w0+w1+w2+w3 <= 1.01);
    assert(src->getWidth()  == dst->getWidth() &&
           src->getHeight() == dst->getHeight());

    dst->fill(0);

    int w = src->getWidth(), h = src->getHeight();
    for(int y = border_y; y < h - border_y; y++)
        for(int x = border_x; x < w - border_x; x++)
        {
            double val =
                (*src)[y][x]
                - w0 * (*src)[y+dy  ][x+dx  ]
                - w1 * (*src)[y+dy  ][x+dx+1]
                - w2 * (*src)[y+dy+1][x+dx  ]
                - w3 * (*src)[y+dy+1][x+dx+1];

            int valint = (int)round(fabs(val));
            (*dst)[y][x] = std::max(std::min(valint, 255), 1);
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
