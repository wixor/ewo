#include <cstdio>
#include <cstring>
#include <cassert>
#include <stdarg.h>
#include <vector>

#include <cairo.h>
#include <cairo-xlib.h>

#include "image.h"
#include "gui.h"

DisplaySlot::DisplaySlot(const char *name)
{
    pthread_mutex_init(&mutex, NULL);
    width = height = 0;
    this->name = name;
    active = false;
}

void DisplaySlot::activate()
{
    if(active) return;
    gui_register(this);
    active = true;
}

void DisplaySlot::deactivate()
{
    if(!active) return;
    gui_unregister(this);
    active = false;
}

DisplaySlot::~DisplaySlot()
{
    assert(!active);
    pthread_mutex_destroy(&mutex);
}

extern "C" void displayslot_paint(struct displayslot *ds_)
{
    DisplaySlot *ds = (DisplaySlot *)ds_;
    ds->lock();
    ds->draw();
    ds->unlock();
}

void DisplaySlot::fill(rgba color)
{
    cairo_set_source_rgba(cr, color.r,color.g,color.b,color.a);
    cairo_paint(cr);
}

void DisplaySlot::drawImage(CairoImage ci, const Matrix *m, bool difference)
{
    cairo_save(cr);

    cairo_matrix_t m1, m2;
    if(m) {
        cairo_matrix_init(&m1,
            (*m)[0][0], (*m)[1][0],
            (*m)[0][1], (*m)[1][1],
            (*m)[0][2], (*m)[1][2]);
        cairo_get_matrix(cr, &m2);
        cairo_matrix_multiply(&m1, &m1, &m2);
        cairo_set_matrix(cr, &m1);
    }

    cairo_surface_t *surface = ci;
    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);
    cairo_set_source(cr, pattern);
    
    cairo_rectangle(cr, 0,0, ci.getWidth(),ci.getHeight());
    cairo_clip(cr);
    
    if(difference)
        cairo_paint_with_alpha(cr, 0.5);
    else
        cairo_paint(cr);

    cairo_restore(cr);

    cairo_pattern_destroy(pattern);
}

void DisplaySlot::drawDot(Point p, float d, rgba color)
{
    cairo_set_source_rgba(cr, 0,0,0,color.a);
    cairo_rectangle(cr, p.x-.5f*d, p.y-.5f*d, d,d);
    cairo_set_source_rgba(cr, color.r,color.g,color.b,1.f);
    cairo_rectangle(cr, p.x-.3f*d, p.y-.3f*d, .6f*d,.6f*d);
    cairo_fill(cr);
}

void DisplaySlot::drawDots(const std::vector<Point> &ps, float d, rgba color)
{
    cairo_set_source_rgba(cr, 0,0,0,color.a);
    for(int i=0; i<(int)ps.size(); i++) 
        cairo_rectangle(cr, ps[i].x-.5f*d, ps[i].y-.5f*d, d,d);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, color.r,color.g,color.b,1.f);
    for(int i=0; i<(int)ps.size(); i++) 
        cairo_rectangle(cr, ps[i].x-.3f*d, ps[i].y-.3f*d, .6f*d,.6f*d);
    cairo_fill(cr);
}

void DisplaySlot::drawSilhouettes(const std::vector<Matrix> &ms, float w, float h, rgba color)
{
    cairo_set_source_rgba(cr, color.r,color.g,color.b,color.a);
    cairo_set_line_width(cr, 1);

    for(int i=0; i<(int)ms.size(); i++) {
        Point p = ms[i] * Point(0,0),
              q = ms[i] * Point(w,0),
              r = ms[i] * Point(w,h),
              s = ms[i] * Point(0,h);
        cairo_move_to(cr, p.x,p.y);
        cairo_line_to(cr, q.x,q.y);
        cairo_line_to(cr, r.x,r.y);
        cairo_line_to(cr, s.x,s.y);
        cairo_close_path(cr);
        cairo_stroke(cr);
    }
}
