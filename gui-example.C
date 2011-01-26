#include <unistd.h>
#include <vector>

#include "util.h"
#include "image.h"
#include "poi.h"
#include "gui.h"

class ColorSlot : public DisplaySlot {
public:
    rgba color;
    ColorSlot(const char *name) : DisplaySlot(name) { }
    ColorSlot(const char *name, rgba color) : DisplaySlot(name), color(color) { }
    virtual ~ColorSlot() { }
    virtual void draw() {
        fill(color);
    }
};

class ImageSlot : public DisplaySlot {
public:
    CairoImage ci;
    Matrix m;

    ImageSlot(const char *name) : DisplaySlot(name) { }
    ImageSlot(const char *name, CairoImage ci) : DisplaySlot(name), ci(ci) { }
    ImageSlot(const char *name, CairoImage ci, Matrix m) : DisplaySlot(name), ci(ci), m(m) { }
    virtual ~ImageSlot() { }
    virtual void draw() {
        resize(ci.getWidth(), ci.getHeight());
        drawImage(ci, m);
    }
};

int main(int argc, char *argv[])
{
    gui_init(&argc, &argv);

    Image im = Image::read(argv[1]);

    CairoImage ci = gui_upload(im);

    ColorSlot red("red", rgba(1,0,0)),
              green("green", rgba(0,1,0));
    ImageSlot ims("image", ci, Matrix::rotation(.2));

    red.activate();
    green.activate();
    ims.activate();

    pause();

    return 0;
}
