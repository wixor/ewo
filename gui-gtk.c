#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gui-gtk.h"

/* ------------------------------------------------------------------------- */

struct displayarea
{
    GtkVBox *vbox;
    GtkLabel *name;
    GtkLabel *caption;
    GtkScrolledWindow *scrolled_window;
    GtkLayout *area;

    struct displayslot *slot;
    int row, col;
};

static GList *slots = NULL, *areas = NULL;
static pthread_mutex_t slots_lock;

static GtkWidget *window, *table, *popup_menu;
static struct displayarea *popup_da;

static cairo_surface_t *transparency_surface;

static void *x11_display;

/* ------------------------------------------------------------------------- */

static gboolean displayarea_expose(GtkWidget *widget, GdkEventExpose *event, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;

    GdkWindow *bin_window = gtk_layout_get_bin_window(da->area);
    cairo_t *cairo = gdk_cairo_create(bin_window);


    if(!da->slot)
    {
        cairo_set_source_rgb(cairo, 1,0,0);
        cairo_paint(cairo);
    }
    else
    {
        pthread_mutex_lock(&da->slot->lock);

        if(!da->slot->cr_surface) {
            cairo_set_source_rgb(cairo, 0,1,0);
            cairo_paint(cairo);
        } else {
            cairo_set_source_surface(cairo, transparency_surface, 0,0);
            cairo_pattern_set_extend(cairo_get_source(cairo), CAIRO_EXTEND_REPEAT);
            cairo_paint(cairo);

            cairo_set_source_surface(cairo, da->slot->cr_surface, 0, 0);
            cairo_paint(cairo);
        }
        
        pthread_mutex_unlock(&da->slot->lock);
    }

    cairo_destroy(cairo);
    return TRUE;
}

static gboolean displayarea_popup(GtkWidget *widget, GdkEventButton *event, gpointer da_)
{
    if(event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        popup_da = (struct displayarea *)da_;
        gtk_menu_popup(GTK_MENU(popup_menu),
                       NULL, NULL, NULL, NULL,
                       event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void displayarea_refresh(struct displayarea *da)
{
    if(da->slot) {
        gtk_layout_set_size(da->area, da->slot->width, da->slot->height);
        gtk_widget_set_size_request(GTK_WIDGET(da->area), da->slot->width, da->slot->height);
    }
    gtk_widget_queue_draw(GTK_WIDGET(da->area));
}

static void displayarea_set_slot(struct displayarea *da, struct displayslot *ds)
{
    da->slot = ds;
    if(ds) {
        gtk_label_set_text(da->name, ds->name);
        gtk_label_set_text(da->caption, ds->caption);
    } else {
        gtk_label_set_text(da->name, "[empty]");
        gtk_label_set_text(da->caption, "[empty]");
    }
    displayarea_refresh(da);
}

static struct displayarea *displayarea_create(void)
{
    struct displayarea *da = malloc(sizeof(struct displayarea));

    da->vbox = GTK_VBOX(gtk_vbox_new(FALSE, 3));

    da->area = GTK_LAYOUT(gtk_layout_new(NULL,NULL));
    da->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL,NULL));
    da->name = GTK_LABEL(gtk_label_new(""));
    da->caption = GTK_LABEL(gtk_label_new(""));

    gtk_misc_set_alignment(GTK_MISC(da->caption), 0,0.5);

    gtk_container_add(GTK_CONTAINER(da->scrolled_window), GTK_WIDGET(da->area));
    gtk_box_pack_start(GTK_BOX(da->vbox), GTK_WIDGET(da->scrolled_window), TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(da->vbox), GTK_WIDGET(da->name), FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(da->vbox), GTK_WIDGET(da->caption), FALSE,FALSE,0);

    gtk_widget_show_all(GTK_WIDGET(da->vbox));

    displayarea_set_slot(da, NULL);
   
    gtk_scrolled_window_set_policy(da->scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); 
    gtk_layout_set_size(da->area, 100, 100);
    gtk_widget_set_size_request(GTK_WIDGET(da->area), 100,100);

    g_signal_connect(G_OBJECT(da->area), "expose_event",
                     G_CALLBACK(displayarea_expose), da);
    g_signal_connect(G_OBJECT(da->area), "button_press_event",
                     G_CALLBACK (displayarea_popup), da);

    gtk_widget_add_events(GTK_WIDGET(da->area), GDK_BUTTON_PRESS_MASK);

    g_object_ref(G_OBJECT(da->vbox));

    return da;
}

static void displayarea_destroy(struct displayarea *da)
{
    gtk_widget_destroy(GTK_WIDGET(da->vbox));
    free(da);
}


/* ------------------------------------------------------------------------- */

enum {
    DS_INIT = 1,
    DS_UPDATE = 2,
    DS_RENAME = 4,
    DS_RECAPTION = 8,
    DS_CLEANUP = 16,
};

static void displayslot_init2(struct displayslot *ds);
static void displayslot_update2(struct displayslot *ds);
static void displayslot_rename2(struct displayslot *ds);
static void displayslot_recaption2(struct displayslot *ds);
static void displayslot_cleanup2(struct displayslot *ds);

static int efd;

static gboolean efd_readable(GIOChannel *chan, GIOCondition cond, gpointer data)
{
    eventfd_t val;
    eventfd_read(efd, &val);

    pthread_mutex_lock(&slots_lock);
    for(GList *p = slots; p; p = p->next)
    {
        struct displayslot *ds = p->data;
        pthread_mutex_lock(&ds->lock);

        if(ds->events & DS_INIT) displayslot_init2(ds);
        if(ds->events & DS_UPDATE) displayslot_update2(ds);
        if(ds->events & DS_RENAME) displayslot_rename2(ds);
        if(ds->events & DS_RECAPTION) displayslot_recaption2(ds);
        if(ds->events & DS_CLEANUP) displayslot_cleanup2(ds);
        ds->events = 0;
        
        pthread_cond_broadcast(&ds->cond);
        pthread_mutex_unlock(&ds->lock);
    }
    pthread_mutex_unlock(&slots_lock);

    return TRUE;
}

static void efd_init(void)
{
    efd = eventfd(0, EFD_CLOEXEC);

    GIOChannel *gioc = g_io_channel_unix_new(efd);
    g_io_add_watch(gioc, G_IO_IN, efd_readable, NULL);
}

static void efd_poke(void)
{
    eventfd_write(efd, 1);
}

/* ------------------------------------------------------------------------- */

static gboolean displayslot_selected(GtkWidget *widget, gpointer ds_)
{
    displayarea_set_slot(popup_da, (struct displayslot *)ds_);
    return TRUE;
}

static void displayslot_init2(struct displayslot *ds)
{
    ds->width = ds->height = ds->stride = -1;
    ds->cr_surface = ds->data = NULL;
    ds->name = strdup("new display slot");
    ds->caption = strdup("");

    ds->menuitem = gtk_menu_item_new_with_label(ds->name);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(popup_menu), GTK_WIDGET(ds->menuitem));
    gtk_widget_show_all(GTK_WIDGET(ds->menuitem));
    
    g_signal_connect(G_OBJECT(ds->menuitem), "activate",
                     G_CALLBACK(displayslot_selected), ds);
}

static void displayslot_cleanup2(struct displayslot *ds)
{
    if(ds->cr_surface)
        cairo_surface_destroy(ds->cr_surface);
    free(ds->data);
    free(ds->name);
    free(ds->caption);
    
    for(GList *p = areas; p; p = p->next) {
        struct displayarea *da = p->data;
        if(da->slot == ds) 
            displayarea_set_slot(da, NULL);
    }

    gtk_widget_destroy(GTK_WIDGET(ds->menuitem));
}

static void displayslot_rename2(struct displayslot *ds)
{
    gtk_menu_item_set_label(GTK_MENU_ITEM(ds->menuitem), ds->name);
    for(GList *p = areas; p; p = p->next) {
        struct displayarea *da = p->data;
        if(da->slot == ds)
            gtk_label_set_text(da->name, ds->name);
    }
}
static void displayslot_recaption2(struct displayslot *ds)
{
    for(GList *p = areas; p; p = p->next) {
        struct displayarea *da = p->data;
        if(da->slot == ds)
            gtk_label_set_text(da->caption, ds->caption);
    }
}
static void displayslot_update2(struct displayslot *ds)
{
    for(GList *p = areas; p; p = p->next) {
        struct displayarea *da = p->data;
        if(da->slot == ds)
            displayarea_refresh(da);
    }
}

static void displayslot_event(struct displayslot *ds, int ev) {
    ds->events |= ev;
    efd_poke();
}

static void displayslot_event_sync(struct displayslot *ds, int ev)
{
    pthread_mutex_lock(&ds->lock);
    displayslot_event(ds, ev);
    pthread_cond_wait(&ds->cond, &ds->lock);
    pthread_mutex_unlock(&ds->lock);
}


void displayslot_init(struct displayslot *ds)
{
    pthread_mutex_init(&ds->lock, NULL);
    pthread_cond_init(&ds->cond, NULL);
    ds->events = 0;

    pthread_mutex_lock(&slots_lock);
    slots = g_list_prepend(slots, ds);
    pthread_mutex_unlock(&slots_lock);

    displayslot_event_sync(ds, DS_INIT);
}

void displayslot_cleanup(struct displayslot *ds)
{
    displayslot_event_sync(ds, DS_CLEANUP);
    
    pthread_mutex_lock(&slots_lock);
    slots = g_list_remove(slots, ds);
    pthread_mutex_unlock(&slots_lock);

    pthread_cond_destroy(&ds->cond);
    pthread_mutex_destroy(&ds->lock);
}

void displayslot_rename(struct displayslot *ds, char *name)
{
    pthread_mutex_lock(&ds->lock);

    free(ds->name);
    ds->name = name;
    displayslot_event(ds, DS_RENAME);

    pthread_mutex_unlock(&ds->lock);
}

void displayslot_recaption(struct displayslot *ds, char *caption)
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
        
    if(ds->width != width || ds->height != height || ds->data == NULL)
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
            uint32_t *dst = (uint32_t *)((uint8_t *)ds->data + y*ds->stride) + x;
            *dst = src ? (0x00010101 * src + 0xff000000) : 0x00000000;
        }

    displayslot_event(ds, DS_UPDATE);
    
    pthread_mutex_unlock(&ds->lock);
}

/* ------------------------------------------------------------------------- */

static void gui_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void gui_empty(struct displayarea *da, int row, int col)
{
    displayarea_set_slot(popup_da, NULL);
}

static void gui_attach_da(struct displayarea *da, int row, int col)
{
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(da->vbox),
                     col,col+1, row,row+1, GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 5,5);
    da->row = row; da->col = col;
}

static void gui_detach_da(struct displayarea *da) {
    gtk_container_remove(GTK_CONTAINER(table), GTK_WIDGET(da->vbox));
}

static gboolean gui_add_column(GtkWidget *widget, gpointer _unused)
{
    int rows, columns;
    g_object_get(G_OBJECT(table), "n-rows", &rows, "n-columns", &columns, NULL);
    columns++;
    gtk_table_resize(GTK_TABLE(table), rows, columns);

    for(int i=0; i<rows; i++) {
        struct displayarea *da = displayarea_create();
        areas = g_list_prepend(areas, da);
        gui_attach_da(da, i, columns-1);
    }

    return TRUE;
}
static gboolean gui_del_column(GtkWidget *widget, gpointer _unused)
{
    int c = popup_da->col;

    for(GList *p = areas; p; p = p->next)
    {
        struct displayarea *da = (struct displayarea *)p->data;
        if(da->col == c) {
            displayarea_destroy(da);
            p->data = NULL;
        } else if(da->col > c) 
            gui_detach_da(da);
    }
    
    for(GList *p = areas; p; p = p->next)
    {
        struct displayarea *da = (struct displayarea *)p->data;
        if(da != NULL && da->col > c) 
            gui_attach_da(da, da->row, da->col-1);
    }

    areas = g_list_remove_all(areas, NULL);
    
    int rows, columns;
    g_object_get(G_OBJECT(table), "n-rows", &rows, "n-columns", &columns, NULL);
    columns--;
    gtk_table_resize(GTK_TABLE(table), rows, columns);

    return TRUE;
}
static gboolean gui_add_row(GtkWidget *widget, gpointer _unused)
{
    int rows, columns;
    g_object_get(G_OBJECT(table), "n-rows", &rows, "n-columns", &columns, NULL);
    rows++;
    gtk_table_resize(GTK_TABLE(table), rows, columns);

    for(int i=0; i<columns; i++) {
        struct displayarea *da = displayarea_create();
        areas = g_list_prepend(areas, da);
        gui_attach_da(da, rows-1, i);
    }

    return TRUE;
}
static gboolean gui_del_row(GtkWidget *widget, gpointer _unused)
{
    int r = popup_da->row;

    for(GList *p = areas; p; p = p->next)
    {
        struct displayarea *da = (struct displayarea *)p->data;
        if(da->row == r) {
            displayarea_destroy(da);
            p->data = NULL;
        } else if(da->row > r) 
            gui_detach_da(da);
    }
    
    for(GList *p = areas; p; p = p->next)
    {
        struct displayarea *da = (struct displayarea *)p->data;
        if(da != NULL && da->row > r) 
            gui_attach_da(da, da->row-1, da->col);
    }

    areas = g_list_remove_all(areas, NULL);
    
    int rows, columns;
    g_object_get(G_OBJECT(table), "n-rows", &rows, "n-columns", &columns, NULL);
    rows--;
    gtk_table_resize(GTK_TABLE(table), rows, columns);

    return TRUE;
}

static void popup_init()
{
    popup_menu = gtk_menu_new();

    GtkWidget *item;
   
    item = gtk_menu_item_new_with_label("[empty]");
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gui_empty), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    item = gtk_menu_item_new_with_label("add row");
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gui_add_row), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    item = gtk_menu_item_new_with_label("add column");
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gui_add_column), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    item = gtk_menu_item_new_with_label("remove row");
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gui_del_row), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    item = gtk_menu_item_new_with_label("remove column");
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gui_del_column), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);

    gtk_widget_show_all(popup_menu);
}

static void table_init()
{
    table = gtk_table_new(1,1,0);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(table));

    struct displayarea *da = displayarea_create();
    areas = g_list_prepend(areas, da);
    gui_attach_da(da,0,0);
}

static void transparency_pattern_init()
{
    int width = 32, height = 32;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

    void *pattern_data = malloc(stride*height);
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            *((uint32_t *)((uint8_t *)pattern_data + y*stride) + x)
                = (((x>>4)^(y>>4)) & 1) ? 0xffe0e0e0 : 0xffc0c0c0;
    
    transparency_surface =
        cairo_image_surface_create_for_data(
                pattern_data, CAIRO_FORMAT_ARGB32, width, height, stride);

}

struct thread_args {
    int *argc;
    char ***argv;

    pthread_mutex_t lock;
    pthread_cond_t cond;
};

static void *gui_thread(void *args_)
{
    struct thread_args *args = (struct thread_args *)args_;

    gtk_init (args->argc, args->argv);

    pthread_mutex_init(&slots_lock, NULL);
    efd_init();
    transparency_pattern_init();
    popup_init();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gui_destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    table_init();
    gtk_widget_show_all(window);

    x11_display = gdk_x11_drawable_get_xdisplay(GDK_DRAWABLE(window->window));

    pthread_mutex_lock(&args->lock);
    pthread_cond_broadcast(&args->cond);
    pthread_mutex_unlock(&args->lock);
    
    gtk_main();

    exit(0);
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

void *gui_gtk_get_x11_display(void) {
    return x11_display;
}

