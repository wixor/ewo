#include <cstdio>
#include <unistd.h>

#include "util.h"
#include "image.h"
#include "poi.h"
#include "gui.h"

class ImageDisplaySlot : public DisplaySlot
{
public:
    CairoImage ci;
    std::vector<Point> ps;
    rgba color;
    int dotsize;

    inline ImageDisplaySlot(const char *name, rgba color)
        : DisplaySlot(name), color(color), dotsize(6) { }
    virtual ~ImageDisplaySlot() { }

    virtual void draw()
    {
        resize(ci.getWidth(), ci.getHeight());
        drawImage(ci);

        drawDots(ps,dotsize,color);
    }
};

int main(int argc, char *argv[])
{
    spawn_worker_threads(2);
    gui_init(&argc, &argv);
    progress = gui_progress;

    Image im = Image::read(argv[1]);

    gui_status("looking for pois");
    
    std::vector<float> scales;
    scales.push_back(1);
    scales.push_back(3);
    scales.push_back(8);

    Array2D<float> eval = evaluateImage(im, scales, 16);
    POIvec all = extractPOIs(eval, 30);
    POIvec pois = filterPOIs(all, 60) ;

    ImageDisplaySlot evalds("evaluation", rgba(1,0,0,1));
    evalds.ci = gui_upload(visualizeEvaluation(eval));
    evalds.activate();

    ImageDisplaySlot imds("image & pois", rgba(.1,1,.1,.5));
    imds.ci = gui_upload(im);
    imds.ps = std::vector<Point>(pois.begin(), pois.end());
    imds.bind();

    gui_status("building proximity info");
    ProximityMap pm;
    pm.resize(im.getWidth(), im.getHeight(), 1, 8);
    pm.build(pois);


    ImageDisplaySlot proxds("proximity", rgba(1,1,1,1));
    proxds.ci = gui_upload(pm.visualize());
    proxds.ps = imds.ps;
    proxds.dotsize = 3;
    proxds.bind();

    info("done");
    
    pause();


    return 0;
}

