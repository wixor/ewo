#ifndef __GTK_GUI_H__
#define __GTK_GUI_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DS_INIT = 1,
    DS_UPDATE = 2,
    DS_RENAME = 4,
    DS_RECAPTION = 8,
    DS_BIND = 16,
    DS_UNBIND = 32,
    DS_CLEANUP = 64,
};

struct displayslot
{
    int events; /* events queued up to this point */
    
    char *name, *caption; /* name and caption */
    void *cr_surface; /* cairo_sufrace_t */

    struct {
        int stamp;
        void *user_data, *user_data2, *user_data3;
    } iter;

    pthread_mutex_t lock;
    pthread_cond_t cond;
};

void gui_gtk_poke();
void gui_gtk_register(struct displayslot *ds);
void gui_gtk_unregister(struct displayslot *ds);
void gui_gtk_init(int *argc, char ***argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
