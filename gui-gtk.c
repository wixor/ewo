#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <gtk/gtk.h>

#include "gui-gtk.h"

/* ------------------------------------------------------------------------- */

struct displayarea
{
    GtkScrolledWindow *scrolled_window;
    GtkLayout *area;
    struct displayslot *slot;
    gboolean bound;
};

static GList *slots = NULL, *areas = NULL;

static GtkWidget *window, *table, *popup_menu;

/* ------------------------------------------------------------------------- */

static gboolean displayarea_expose(GtkWidget *widget, GdkEventExpose *event, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;

    GdkWindow *bin_window = gtk_layout_get_bin_window(da->area);
    cairo_t *cairo = gdk_cairo_create(bin_window);

    cairo_set_source_rgb(cairo, 1,0,0);
    cairo_paint(cairo);

    if(da->slot && da->slot->cr_surface) {
        cairo_set_source_surface(cairo, da->slot->cr_surface, 0, 0);
        cairo_paint(cairo);
    }

    cairo_destroy(cairo);
    return TRUE;
}

static gboolean displayarea_popup(GtkWidget *widget, GdkEventButton *event, gpointer da_)
{
    if(event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        printf("here!\n");
        gtk_menu_popup(GTK_MENU(popup_menu),
                       NULL, NULL, NULL, NULL,
                       event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void displayarea_bind(struct displayarea *da, int row, int col)
{
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(da->scrolled_window),
                     col,col+1, row,row+1, GTK_EXPAND, GTK_EXPAND, 0, 0);
    da->bound = TRUE;
}

static void displayarea_unbind(struct displayarea *da)
{
    gtk_container_remove(GTK_CONTAINER(table), GTK_WIDGET(da->scrolled_window));
    da->bound = FALSE;
}

static void displayarea_refresh(struct displayarea *da)
{
    gtk_widget_queue_draw(GTK_WIDGET(da->area));
}

static struct displayarea *displayarea_create(void)
{
    struct displayarea *da = malloc(sizeof(struct displayarea));

    da->slot = NULL;

    da->area = GTK_LAYOUT(gtk_layout_new(NULL,NULL));
    da->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL,NULL));

    gtk_widget_set_size_request(GTK_WIDGET(da->area), 100, 100);

    gtk_container_add(GTK_CONTAINER(da->scrolled_window), GTK_WIDGET(da->area));
    gtk_widget_show(GTK_WIDGET(da->area));
    gtk_widget_show(GTK_WIDGET(da->scrolled_window));

    g_signal_connect(G_OBJECT(da->area), "expose_event",
                     G_CALLBACK(displayarea_expose), da);
    g_signal_connect(G_OBJECT(da->area), "button_press_event",
                     G_CALLBACK (displayarea_popup), da);

    gtk_widget_add_events(GTK_WIDGET(da->area), GDK_BUTTON_PRESS_MASK);

    g_object_ref(G_OBJECT(da->scrolled_window));
    areas = g_list_prepend(areas, ds);

    return da;
}

static void displayarea_destroy(struct displayarea *da)
{
    if(da->bound)
        displayarea_unbind();
    g_object_unref(G_OBJECT(da->scrolled_window));
    areas = g_list_remove(areas, da);

    free(da);
}


/* ------------------------------------------------------------------------- */

static int efd;

static gboolean efd_readable(GIOChannel *chan, GIOCondition cond, gpointer data)
{
    eventfd_t val;
    eventfd_read(efd, &val);


    return TRUE;
}

static void efd_init(void)
{
    int efd = eventfd(0, EFD_CLOEXEC);

    GIOChannel *gioc = g_io_channel_unix_new(efd);
    g_io_add_watch(gioc, G_IO_IN, efd_readable, NULL);
}

static void efd_poke(void)
{
    eventfd_write(efd, 1);
}

/* ------------------------------------------------------------------------- */

static void displayslot_init2(struct displayslot *ds)
{
    ds->width = ds->height = ds->stride = -1;
    ds->cr_surface = ds->data = NULL;
    ds->name = strdup("new display slot");
    ds->caption = NULL;

    ds->menuitem = gtk_menu_item_new_with_label(ds->name);
    gtk_menu_append(GTK_MENU(popup_menu), GTK_MENU_ITEM(ds->menuitem));
}

static void displayslot_cleanup2(struct displayslot *ds)
{
    if(ds->cr_surface)
        cairo_surface_destroy(ds->cr_surface);
    free(ds->data);
    free(ds->name);
    free(ds->caption);

    gtk_widget_destroy(GTK_WIDGET(ds->menuitem));
}

static void displayslot_redraw(struct displayslot *ds)
{
    GList *p;
    for(p = areas; p; p = p->next) {
        struct displayarea *da = p->gdata;
        if(da->slot == ds)

    }

}

static void displayslot_rename2(struct displayslot *ds) {
    gtk_menu_item_set_label(GTK_MENU_ITEM(ds->menuitem), ds->name);
    displayslot_redraw(ds);
}
static void displayslot_recaption2(struct displayslot *ds) {
    displayslot_redraw(ds);
}
static void displayslot_update2(struct displayslot *ds) {
    displayslot_redraw(ds);
}


static void displayslot_event(struct displayslot *ds, int ev) {
    ds->events |= ev;
    efd_poke();
}


void displayslot_init(struct displayslot *ds)
{
    pthread_mutex_init(&ds->lock, NULL);
    pthread_cond_init(&ds->cond, NULL);

    pthread_mutex_lock(&ds->lock);
   
    pthread_mutex_lock(&slots_lock);
    slots = g_list_prepend(slots, ds);
    pthread_mutex_unlock(&slots_lock);

    displayslot_event(ds, DS_INIT);
    pthread_cond_wait(&ds->cond, &ds->lock);
    
    pthread_mutex_unlock(&ds->lock);
}

void displayslot_cleanup(struct displayslot *ds)
{
    pthread_mutex_lock(&ds->lock);

    displayslot_event(ds, DS_DESTROY);
    pthread_cond_wait(&ds->cond, &ds->lock);
    
    pthread_mutex_lock(&slots_lock);
    slots = g_list_remove(slots, ds);
    pthread_mutex_unlock(&slots_lock);

    pthread_mutex_unlock(&ds->lock);
    
    pthread_cond_destroy(&ds->cond);
    pthread_mutex_destroy(&ds->lock);
}



void displayslot_rename(struct displayslot *ds, const char *name)
{
    pthread_mutex_lock(&ds->lock);

    free(ds->name);
    ds->name = name;
    displayslot_event(ds, DS_RENAME);

    pthread_mutex_unlock(&ds->lock);
}

void displayslot_recaption(struct displayslot *ds, const char *caption)
{
    pthread_mutex_lock(&ds->lock);

    free(ds->caption);
    ds->caption = caption;
    displayslot_event(ds, DS_RECAPTION);

    pthread_mutex_unlock(&ds->lock);
}

void displayslot_update(struct displayslot *ds, int width, int height, const void *data)
{
    pthread_mutex_lock(&ds->lock);
        
    if(ds->width != width || ds->height != height)
    {
        if(ds->cr_surface)
            cairo_surface_destroy(ds->cr_surface);

        int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

        ds->data = realloc(ds->data, height*stride);
        ds->width = width;
        ds->height = height;
        ds->stride = stride;

        ds->cr_surface =
            cairo_image_surface_create_for_data(
                    ds->data, CAIRO_FORMAT_ARGB32, width, height, stride);
    }

    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
        {
            uint8_t src = ((uint8_t *)data)[x + y*width];
            uint32_t *dst = (uint32_t *)(ds->data + y*ds->stride) + x;
            *dst = src ? (0x00010101 * src + 0xff000000) : 0x00000000;
        }

    displayslot_event(ds, DS_UPDATE);
    
    pthread_mutex_unlock(&ds->lock);
}

/* ------------------------------------------------------------------------- */

struct thread_args {
    int *argc;
    char ***argv;

    pthread_mutex_t lock;
    pthread_cond_t cond;
};

static void gui_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void *gui_thread(void *args_)
{
    struct thread_args *args = (struct thread_args *)args_;

    gtk_init (args->argc, args->argv);

    efd_init();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gui_destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    table = gtk_table_new(1,1,0);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(table));
    gtk_widget_show_all(window);

    popup_menu = GTK_MENU(gtk_menu_new());

    struct displayarea *da = displayarea_create();
    displayarea_bind(da, 0,0);

    pthread_mutex_lock(&args->lock);
    pthread_cond_broadcast(&args->cond);
    pthread_mutex_unlock(&args->lock);
    
    gtk_main();
    
    return NULL;
}

void gui_gtk_init(int *argc, char ***argv)
{
    pthread_t thr;
    struct thread_args args = {
        .argc = argc,
        .argv = argv
    };

    pthread_mutex_init(&args.lock, NULL);
    pthread_cond_init(&args.cond, NULL);

    pthread_mutex_lock(&args.lock);
    pthread_create(&thr, NULL, gui_thread, &args);
    pthread_cond_wait(&args.cond, &args.lock);
    pthread_mutex_unlock(&args.lock);

    pthread_cond_destroy(&args.cond);
    pthread_mutex_destroy(&args.lock);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    gui_gtk_init(&argc, &argv);
    pause();
    return 0;
}
