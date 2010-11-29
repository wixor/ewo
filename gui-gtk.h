#ifndef __GTK_GUI_H__
#define __GTK_GUI_H__

#include <pthread.h>

enum {
    DS_CREATE = 1,
    DS_UPDATE = 2,
    DS_RENAME = 4,
    DS_RECAPTION = 8,
    DS_DESTROY = 16,
};

struct displayslot
{
    const char *name, *caption;
    int events;
    
    int width, height, stride;
    void *data, *cr_surface; /* cairo_sufrace_t *cr_surface, actually */

    void *menuitem; /* GtkMenuItem *menuitem, actually */

    pthread_mutex_t lock;
    pthread_cond_t cond;
};

#ifdef __cplusplus
extern "C" {
#endif

void displayslot_init(struct displayslot *ds);
void displayslot_cleanup(struct displayslot *ds);
void displayslot_rename(struct displayslot *ds, const char *name);
void displayslot_recaption(struct displayslot *ds, const char *caption);
void displayslot_update(struct displayslot *ds, int width, int height, const void *data);

void gui_gtk_init(int *argc, char ***argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
