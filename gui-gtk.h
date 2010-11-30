#ifndef __GTK_GUI_H__
#define __GTK_GUI_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct displayslot
{
    int events;
    
    int width, height, stride;
    void *data, *cr_surface; /* cairo_sufrace_t *cr_surface, actually */

    char *name, *caption;
    void *menuitem; /* GtkMenuItem *menuitem, actually */

    pthread_mutex_t lock;
    pthread_cond_t cond;
};


void displayslot_init(struct displayslot *ds);
void displayslot_cleanup(struct displayslot *ds);
void displayslot_rename(struct displayslot *ds, char *name);
void displayslot_recaption(struct displayslot *ds, char *caption);
void displayslot_update(struct displayslot *ds, int width, int height, const void *data);

void gui_gtk_init(int *argc, char ***argv);
void *gui_gtk_get_x11_display(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
