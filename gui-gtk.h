#ifndef __GTK_GUI_H__
#define __GTK_GUI_H__

#include <pthread.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GTK_H__
struct _GtkTreeIter {
    int stamp;
    void *user_data, *user_data2, *user_data3;
};
typedef struct _GtkTreeIter GtkTreeIter;
#endif

struct displayslot
{
    pthread_mutex_t mutex;

    cairo_t *cr;
    int width, height;
    const char *name;

    GtkTreeIter iter;
};

void gui_register(struct displayslot *);
void gui_unregister(struct displayslot *);
void gui_bind(struct displayslot *);
void gui_unbind(struct displayslot *);

/* do not use this, see gui_upload from gui.h */
cairo_surface_t *gui_do_upload(int width, int height, const void *bytes, int gray);

void gui_status(const char *fmt, ...);
void gui_progress(float value);
void gui_init(int *argc, char ***argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
