#ifndef __GTK_GUI_H__
#define __GTK_GUI_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct displayslot
{
    int events; /* events queued up to this point */
    
    char *name, *caption; /* pointers to temporary storage where new name or new caption is kept */

    int width, height, stride; /* the surface data parameters */
    void *data; /* the surface data */
    void *cr_surface; /* the cairo_sufrace_t * */

    struct {
        int stamp;
        void *user_data, *user_data2, *user_data3;
    } iter;

    pthread_mutex_t lock;
    pthread_cond_t cond;
};

void displayslot_init(struct displayslot *ds, char *name);
void displayslot_cleanup(struct displayslot *ds);
void displayslot_rename(struct displayslot *ds, char *name);
void displayslot_recaption(struct displayslot *ds, char *caption);
void displayslot_update(struct displayslot *ds, int width, int height, const void *data);
void displayslot_bind(struct displayslot *ds);
void displayslot_unbind(struct displayslot *ds);

void gui_gtk_init(int *argc, char ***argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
