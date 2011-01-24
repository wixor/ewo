#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include "gui-gtk.h"

/* ------------------------------------------------------------------------- */

struct displayarea
{
    GtkVBox *root;
      GtkScrolledWindow *scrolled_window;
        GtkLayout *area;
      GtkHBox *buttbox;
        GtkComboBox *slotselect;
        GtkButton *zoomin;
        GtkButton *zoomout;

    struct displayslot *slot;
    float scale;
};

static struct displayarea areas[4];

static GtkListStore *slotsModel;

static GtkToolbar *toolbar;
static GtkToolButton *single, *hsplit, *vsplit, *quad;

static int statusbar_ctx;
static GtkStatusbar *statusbar;

/* ------------------------------------------------------------------------- */

/* this is defined in gui.C */
void displayslot_paint(struct displayslot *);

static gboolean displayarea_expose(GtkWidget *widget, GdkEventExpose *event, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;

    guint expected_width, expected_height, current_width, current_height;
    gtk_layout_get_size(da->area, &current_width, &current_height);

    GdkWindow *bin_window = gtk_layout_get_bin_window(da->area);
    cairo_t *cairo = gdk_cairo_create(bin_window);

    if(!da->slot) {
        cairo_set_source_rgb(cairo, .85,.85,.85);
        cairo_paint(cairo);
        expected_width = expected_height = 0;
    } else {
        struct displayslot *ds = da->slot;

        cairo_matrix_t matrix;
        cairo_matrix_init_scale(&matrix, da->scale, da->scale);
        cairo_set_matrix(cairo, &matrix);

        ds->cr = cairo;
        displayslot_paint(da->slot);

        expected_width = ds->width * da->scale;
        expected_height = ds->height * da->scale;
    }
    
    cairo_destroy(cairo);

    if(expected_width != current_width || expected_height != current_height)
        gtk_layout_set_size(da->area, expected_width, expected_height);
    
    return TRUE;
}

static void displayarea_zoomin(GtkWidget *widget, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;
    if(da->scale < 10) {
        da->scale *= 2.f;
        if(da->slot)
            gtk_widget_queue_draw(GTK_WIDGET(da->area));
    }
}
static void displayarea_zoomout(GtkWidget *widget, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;
    if(da->scale > .125) {
        da->scale *= 0.5f;
        if(da->slot)
            gtk_widget_queue_draw(GTK_WIDGET(da->area));
    }
}

static void displayarea_refresh(struct displayarea *da)
{
    if(da->slot)
        gtk_widget_queue_draw(GTK_WIDGET(da->area));
}

static void displayarea_set_slot(struct displayarea *da, struct displayslot *ds)
{
    da->slot = ds;
    gtk_widget_queue_draw(GTK_WIDGET(da->area));
}

static void displayarea_change_slot(struct displayarea *da, struct displayslot *ds)
{
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(slotsModel), &iter);
    do {
        struct displayslot *ds_;
        gtk_tree_model_get(GTK_TREE_MODEL(slotsModel), &iter,  0,&ds_,  -1);
        if(ds_ == ds) {
            gtk_combo_box_set_active_iter(da->slotselect, &iter);
            break;
        }
    } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(slotsModel), &iter));
}

static void displayarea_slot_changed(GtkWidget *widget, gpointer da_)
{
    struct displayarea *da = (struct displayarea *)da_;

    GtkTreeIter iter;
    if(!gtk_combo_box_get_active_iter(da->slotselect, &iter))
        return;

    struct displayslot *ds;
    gtk_tree_model_get(GTK_TREE_MODEL(slotsModel), &iter,  0,&ds,  -1);
    displayarea_set_slot(da, ds);
}

static void displayarea_init(struct displayarea *da)
{
    da->scale = 1;

    da->root = GTK_VBOX(gtk_vbox_new(FALSE, 3));
    da->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL,NULL));
    da->area = GTK_LAYOUT(gtk_layout_new(NULL,NULL));
    da->buttbox = GTK_HBOX(gtk_hbox_new(FALSE, 3));
    da->slotselect = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(slotsModel)));
    da->zoomin = GTK_BUTTON(gtk_button_new());
    da->zoomout = GTK_BUTTON(gtk_button_new());

    gtk_button_set_image(da->zoomin,  gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN,  GTK_ICON_SIZE_BUTTON));
    gtk_button_set_image(da->zoomout, gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_BUTTON));

    {
        GtkCellRenderer *crend = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(da->slotselect), crend, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(da->slotselect), crend, "text", 1, NULL);
    }

    gtk_box_pack_start(GTK_BOX(da->root), GTK_WIDGET(da->scrolled_window), TRUE,TRUE,2);
    gtk_box_pack_start(GTK_BOX(da->root), GTK_WIDGET(da->buttbox), FALSE,FALSE,2);

    gtk_container_add(GTK_CONTAINER(da->scrolled_window), GTK_WIDGET(da->area));

    gtk_box_pack_start(GTK_BOX(da->buttbox), GTK_WIDGET(da->slotselect), TRUE,TRUE,2);
    gtk_box_pack_start(GTK_BOX(da->buttbox), GTK_WIDGET(da->zoomin), FALSE,FALSE,2);
    gtk_box_pack_start(GTK_BOX(da->buttbox), GTK_WIDGET(da->zoomout), FALSE,FALSE,2);

    gtk_widget_show_all(GTK_WIDGET(da->root));

    gtk_scrolled_window_set_policy(da->scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); 
    gtk_scrolled_window_set_shadow_type(da->scrolled_window, GTK_SHADOW_IN);

    g_signal_connect(G_OBJECT(da->area), "expose_event",
                     G_CALLBACK(displayarea_expose), da);
    g_signal_connect(G_OBJECT(da->zoomin), "clicked",
                     G_CALLBACK (displayarea_zoomin), da);
    g_signal_connect(G_OBJECT(da->zoomout), "clicked",
                     G_CALLBACK (displayarea_zoomout), da);
    g_signal_connect(G_OBJECT(da->slotselect), "changed",
                     G_CALLBACK (displayarea_slot_changed), da);

    g_object_ref(G_OBJECT(da->root));
}

static void displayarea_cleanup(struct displayarea *da)
{
    gtk_widget_destroy(GTK_WIDGET(da->root));
}

/* ------------------------------------------------------------------------- */

/* these are tentative definitions (ever heard of them?), supplemented with
 * initialization data at the end of this file. */
static const guint8 icoSingle[] __attribute__ ((__aligned__ (4)));
static const guint8 icoVsplit[] __attribute__ ((__aligned__ (4)));
static const guint8 icoHsplit[] __attribute__ ((__aligned__ (4)));
static const guint8 icoQuad[] __attribute__ ((__aligned__ (4)));

void gui_bind(struct displayslot *ds)
{
    gdk_threads_enter();

    gboolean bound = FALSE;
    for(int i=0; i<4; i++)
        bound = bound || (areas[i].slot == ds);

    if(!bound)
        for(int i=0; i<4; i++)
            if(areas[i].slot == NULL) {
                displayarea_change_slot(&areas[i], ds);
                break;
            }

    gdk_flush();
    gdk_threads_leave();
}

void gui_unbind(struct displayslot *ds)
{
    gdk_threads_enter();
    
    for(int i=0; i<4; i++)
        if(areas[i].slot == ds)
            displayarea_change_slot(&areas[i], NULL);

    gdk_flush();
    gdk_threads_leave();
}

void gui_register(struct displayslot *ds)
{
    gdk_threads_enter();
    
    gtk_list_store_insert_with_values(
        slotsModel,&ds->iter,2000000000,  0,ds,  1,ds->name, -1);

    gdk_flush();
    gdk_threads_leave();
}

void gui_unregister(struct displayslot *ds)
{
    gdk_threads_enter();

    for(int i=0; i<4; i++)
        if(areas[i].slot == ds)
            displayarea_change_slot(&areas[i], NULL);
    gtk_list_store_remove(slotsModel, &ds->iter);

    gdk_flush();
    gdk_threads_leave();
}

void gui_status(const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    char *buf; vasprintf(&buf, fmt, args);
    va_end(args);

    gdk_threads_enter();
    
    gtk_statusbar_pop(statusbar, statusbar_ctx);
    gtk_statusbar_push(statusbar, statusbar_ctx, buf);
    
    gdk_flush();
    gdk_threads_leave();

    free(buf);
}

static cairo_user_data_key_t gui_free_pixmap_key;
static void gui_free_pixmap(void *data)
{
    gdk_threads_enter();

    Display *display =
        gdk_x11_get_default_xdisplay();
    Pixmap pixmap =
        cairo_xlib_surface_get_drawable((cairo_surface_t *)data);

    XFreePixmap(display, pixmap);
    
    gdk_flush();
    gdk_threads_leave();
}

cairo_surface_t *img_makesurface(int width, int height, const uint8_t *bytes); /* from image.c */
cairo_surface_t *gui_do_upload(int width, int height, const void *bytes)
{
    cairo_surface_t *source = img_makesurface(width, height, bytes);
    if(!source) return NULL;

    gdk_threads_enter();

    Display *display =
        gdk_x11_get_default_xdisplay();
    Window root =
        XDefaultRootWindow(display);
    Visual *vis =
        gdk_x11_visual_get_xvisual(
            gdk_visual_get_best_with_both(24, GDK_VISUAL_TRUE_COLOR));
    
    Pixmap pixmap =
        XCreatePixmap(display, root, width, height, 24);
    cairo_surface_t *surface =
        cairo_xlib_surface_create(display, pixmap, vis, width, height);
    
    cairo_surface_set_user_data(surface, &gui_free_pixmap_key, surface, gui_free_pixmap);

    cairo_t *cr = cairo_create(surface);
    cairo_set_source_surface(cr, source, 0,0);
    cairo_paint(cr);
    cairo_destroy(cr);

    gdk_flush();
    gdk_threads_leave();

    cairo_surface_destroy(source);

    return surface;
}

static gboolean gui_refresh(gpointer data)
{
    gdk_threads_enter();

    for(int i=0; i<4; i++)
        displayarea_refresh(&areas[i]);

    gdk_flush();
    gdk_threads_leave();
    return TRUE;
}

static void gui_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void gui_toggle_split_mode(GtkWidget *widget, gpointer _mode)
{
    int mode = (int)_mode;
    gtk_widget_show(GTK_WIDGET(areas[0].root));
    gtk_widget_set_visible(GTK_WIDGET(areas[1].root), mode == 1 || mode == 3);
    gtk_widget_set_visible(GTK_WIDGET(areas[2].root), mode == 2 || mode == 3);
    gtk_widget_set_visible(GTK_WIDGET(areas[3].root), mode == 3);
}

static GtkToolButton *gui_make_split_btn(const guint8 icon[], int mode, int idx)
{
    GtkWidget *img = gtk_image_new_from_pixbuf(
        gdk_pixbuf_new_from_inline(-1, icon, TRUE, NULL));
    GtkToolButton *btn = GTK_TOOL_BUTTON(gtk_tool_button_new(img, NULL));
    
    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(btn), idx);

    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(gui_toggle_split_mode), (void *)mode);

    return btn;
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

    g_thread_init(NULL);
    gdk_threads_init();

    gtk_init (args->argc, args->argv);

    slotsModel = gtk_list_store_new(4, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    { GtkTreeIter iter;
      gtk_list_store_insert_with_values(slotsModel,&iter,0,  0,NULL,  1,"", -1); }

    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gui_destroy), NULL);

    displayarea_init(&areas[0]);
    displayarea_init(&areas[1]);
    displayarea_init(&areas[2]);
    displayarea_init(&areas[3]);

    statusbar = GTK_STATUSBAR(gtk_statusbar_new());
    statusbar_ctx = gtk_statusbar_get_context_id(statusbar,"");

    toolbar = GTK_TOOLBAR(gtk_toolbar_new());
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    single  = gui_make_split_btn(icoSingle, 0, 0);
    vsplit  = gui_make_split_btn(icoVsplit, 1, 1);
    hsplit  = gui_make_split_btn(icoHsplit, 2, 2);
    quad    = gui_make_split_btn(icoQuad,   3, 3);

    GtkWidget *table = gtk_table_new(2,2,0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(areas[0].root), 0,1,0,1,GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,3,3);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(areas[1].root), 1,2,0,1,GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,3,3);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(areas[2].root), 0,1,1,2,GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,3,3);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(areas[3].root), 1,2,1,2,GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,3,3);
    
    GtkWidget *vbox = gtk_vbox_new(0,0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(toolbar),   FALSE, FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox), table,                 TRUE,  TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(statusbar), FALSE, FALSE,0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    gtk_widget_show_all(window);
    gui_toggle_split_mode(NULL, (void *)0);

    g_timeout_add(500, gui_refresh, NULL);

    pthread_mutex_lock(&args->lock);
    pthread_cond_signal(&args->cond);
    pthread_mutex_unlock(&args->lock);

    gdk_threads_enter();    
    gtk_main();
    gdk_threads_leave();

    _exit(0); /* ugly. this is because globally-allocated displayslots will want to
                 de-degister themselves upon exit(), and since we won't be running
                 any more, they'll hang up. */
}

void gui_init(int *argc, char ***argv)
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

static const guint8 icoSingle[] __attribute__ ((__aligned__ (4))) = 
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (626) */
  "\0\0\2\212"
  /* pixdata_type (0x2010001) */
  "\2\1\0\1"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (32) */
  "\0\0\0\40"
  /* height (32) */
  "\0\0\0\40"
  /* pixel_data: */
  "\2PPPMMM\234PPP\4MMMPPPMMM\330\330\330\234\354\354\354\4\330\330\330"
  "MMMNNN\353\353\353\234\372\372\372\4\353\353\353NNNMMN\353\353\353\234"
  "\372\372\372\1\353\353\353\202MMM\1\353\353\353\234\372\372\372\4\353"
  "\353\353MMMLLL\352\352\352\234\372\372\372\5\352\352\352LLLKKK\351\351"
  "\351\371\371\371\232\372\372\372\5\371\371\371\351\351\351KKKJJJ\351"
  "\351\351\234\371\371\371\5\351\351\351JJJJII\351\350\350\370\370\370"
  "\232\371\371\371\5\370\370\370\351\350\351JJIIII\350\350\350\234\370"
  "\370\370\4\350\350\350IIIHHH\347\347\347\234\370\370\370\5\347\347\347"
  "HHHGGG\346\346\346\367\367\367\232\370\370\370\5\367\367\367\346\346"
  "\346GGGFFF\345\345\345\234\367\367\367\1\345\345\345\202FFF\2\345\345"
  "\345\367\366\366\232\367\367\367\6\367\367\366\345\345\345FFFEEE\345"
  "\345\345\366\366\366\232\367\367\367\6\366\366\366\345\345\345EEEDDD"
  "\344\344\344\365\365\365\232\366\366\366\5\365\365\365\344\344\344DD"
  "DCCC\343\343\343\234\365\365\365\5\343\343\343CCCBBC\342\343\342\364"
  "\364\364\232\365\365\365\5\364\364\364\343\343\342CCCBBB\342\342\342"
  "\234\364\364\364\4\342\342\342BBBAAA\341\341\341\234\364\364\364\5\341"
  "\341\341AAA@@@\340\340\340\363\363\363\232\364\364\364\5\363\363\363"
  "\340\340\340@@@\77\77\77\337\337\337\234\363\363\363\1\337\337\337\202"
  "\77\77\77\2\337\337\337\362\362\362\232\363\363\363\6\362\362\363\337"
  "\337\337\77\77\77>>>\336\336\336\362\362\362\232\363\363\363\5\362\362"
  "\362\336\336\336>>>===\336\336\336\234\362\362\362\5\336\336\336===<"
  "<<\335\335\335\361\361\361\232\362\362\362\6\361\361\361\335\335\335"
  "<<<;;;\335\334\334\360\360\360\232\361\361\361\5\360\360\360\335\335"
  "\335<<;;;;\334\334\334\234\360\360\360\4\334\334\334;;;:::\333\333\333"
  "\234\360\360\360\5\333\333\333:::999\330\330\330\356\356\356\232\357"
  "\357\357\6\356\356\356\330\330\330999777\271\271\271\330\327\327\232"
  "\330\330\330\5\330\327\330\271\271\271777888555\234888\2""555888"};


static const guint8 icoVsplit[] __attribute__ ((__aligned__ (4))) = 
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (956) */
  "\0\0\3\324"
  /* pixdata_type (0x2010001) */
  "\2\1\0\1"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (32) */
  "\0\0\0\40"
  /* height (32) */
  "\0\0\0\40"
  /* pixel_data: */
  "\2PPPMMM\234PPP\4MMMPPPMMM\330\330\330\215\354\354\354\2bbb\337\337\337"
  "\215\354\354\354\4\330\330\330MMMNNN\353\353\353\215\372\372\372\2bb"
  "b\337\337\337\215\372\372\372\4\353\353\353NNNMMN\353\353\353\215\372"
  "\372\372\2bbb\337\337\337\215\372\372\372\1\353\353\353\202MMM\1\353"
  "\353\353\215\372\372\372\2bbb\337\337\337\215\372\372\372\4\353\353\353"
  "MMMLLL\352\352\352\215\372\372\372\2bbb\337\337\337\215\372\372\372\5"
  "\352\352\352LLLKKK\351\351\351\371\371\371\214\372\372\372\2bbb\337\337"
  "\337\214\372\372\372\5\371\371\371\351\351\351KKKJJJ\351\351\351\215"
  "\371\371\371\2bbb\337\337\337\215\371\371\371\5\351\351\351JJJJII\351"
  "\350\350\370\370\370\214\371\371\371\2bbb\337\337\337\214\371\371\371"
  "\5\370\370\370\351\350\351JJIIII\350\350\350\215\370\370\370\2bbb\337"
  "\337\337\215\370\370\370\4\350\350\350IIIHHH\347\347\347\215\370\370"
  "\370\2bbb\337\337\337\215\370\370\370\5\347\347\347HHHGGG\346\346\346"
  "\367\367\367\214\370\370\370\2bbb\337\337\337\214\370\370\370\5\367\367"
  "\367\346\346\346GGGFFF\345\345\345\215\367\367\367\2bbb\337\337\337\215"
  "\367\367\367\1\345\345\345\202FFF\2\345\345\345\367\366\366\214\367\367"
  "\367\2bbb\337\337\337\214\367\367\367\6\367\367\366\345\345\345FFFEE"
  "E\345\345\345\366\366\366\214\367\367\367\2bbb\337\337\337\214\367\367"
  "\367\6\366\366\366\345\345\345EEEDDD\344\344\344\365\365\365\214\366"
  "\366\366\2bbb\337\337\337\214\366\366\366\5\365\365\365\344\344\344D"
  "DDCCC\343\343\343\215\365\365\365\2bbb\337\337\337\215\365\365\365\5"
  "\343\343\343CCCBBC\342\343\342\364\364\364\214\365\365\365\2bbb\337\337"
  "\337\214\365\365\365\5\364\364\364\343\343\342CCCBBB\342\342\342\215"
  "\364\364\364\2bbb\337\337\337\215\364\364\364\4\342\342\342BBBAAA\341"
  "\341\341\215\364\364\364\2bbb\337\337\337\215\364\364\364\5\341\341\341"
  "AAA@@@\340\340\340\363\363\363\214\364\364\364\2bbb\337\337\337\214\364"
  "\364\364\5\363\363\363\340\340\340@@@\77\77\77\337\337\337\215\363\363"
  "\363\2bbb\337\337\337\215\363\363\363\1\337\337\337\202\77\77\77\2\337"
  "\337\337\362\362\362\214\363\363\363\2bbb\337\337\337\214\363\363\363"
  "\6\362\362\363\337\337\337\77\77\77>>>\336\336\336\362\362\362\214\363"
  "\363\363\2bbb\337\337\337\214\363\363\363\5\362\362\362\336\336\336>"
  ">>===\336\336\336\215\362\362\362\2bbb\337\337\337\215\362\362\362\5"
  "\336\336\336===<<<\335\335\335\361\361\361\214\362\362\362\2bbb\337\337"
  "\337\214\362\362\362\6\361\361\361\335\335\335<<<;;;\335\334\334\360"
  "\360\360\214\361\361\361\2bbb\337\337\337\214\361\361\361\5\360\360\360"
  "\335\335\335<<;;;;\334\334\334\215\360\360\360\2bbb\337\337\337\215\360"
  "\360\360\4\334\334\334;;;:::\333\333\333\215\360\360\360\2bbb\337\337"
  "\337\215\360\360\360\5\333\333\333:::999\330\330\330\356\356\356\214"
  "\357\357\357\2bbb\337\337\337\214\357\357\357\6\356\356\356\330\330\330"
  "999777\271\271\271\330\327\327\214\330\330\330\2bbb\337\337\337\214\330"
  "\330\330\5\330\327\330\271\271\271777888555\234888\2""555888"};


static const guint8 icoHsplit[] __attribute__ ((__aligned__ (4))) = 
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (608) */
  "\0\0\2x"
  /* pixdata_type (0x2010001) */
  "\2\1\0\1"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (32) */
  "\0\0\0\40"
  /* height (32) */
  "\0\0\0\40"
  /* pixel_data: */
  "\2PPPMMM\234PPP\4MMMPPPMMM\330\330\330\234\354\354\354\4\330\330\330"
  "MMMNNN\353\353\353\234\372\372\372\4\353\353\353NNNMMN\353\353\353\234"
  "\372\372\372\1\353\353\353\202MMM\1\353\353\353\234\372\372\372\4\353"
  "\353\353MMMLLL\352\352\352\234\372\372\372\5\352\352\352LLLKKK\351\351"
  "\351\371\371\371\232\372\372\372\5\371\371\371\351\351\351KKKJJJ\351"
  "\351\351\234\371\371\371\5\351\351\351JJJJII\351\350\350\370\370\370"
  "\232\371\371\371\5\370\370\370\351\350\351JJIIII\350\350\350\234\370"
  "\370\370\4\350\350\350IIIHHH\347\347\347\234\370\370\370\5\347\347\347"
  "HHHGGG\346\346\346\367\367\367\232\370\370\370\5\367\367\367\346\346"
  "\346GGGFFF\345\345\345\234\367\367\367\1\345\345\345\202FFF\2\345\345"
  "\345\367\366\366\232\367\367\367\6\367\367\366\345\345\345FFFEEE\345"
  "\345\345\366\366\366\232\367\367\367\4\366\366\366\345\345\345EEEDDD"
  "\236bbb\2DDDCCC\236\337\337\337\4CCCBBC\342\343\342\364\364\364\232\365"
  "\365\365\5\364\364\364\343\343\342CCCBBB\342\342\342\234\364\364\364"
  "\4\342\342\342BBBAAA\341\341\341\234\364\364\364\5\341\341\341AAA@@@"
  "\340\340\340\363\363\363\232\364\364\364\5\363\363\363\340\340\340@@"
  "@\77\77\77\337\337\337\234\363\363\363\1\337\337\337\202\77\77\77\2\337"
  "\337\337\362\362\362\232\363\363\363\6\362\362\363\337\337\337\77\77"
  "\77>>>\336\336\336\362\362\362\232\363\363\363\5\362\362\362\336\336"
  "\336>>>===\336\336\336\234\362\362\362\5\336\336\336===<<<\335\335\335"
  "\361\361\361\232\362\362\362\6\361\361\361\335\335\335<<<;;;\335\334"
  "\334\360\360\360\232\361\361\361\5\360\360\360\335\335\335<<;;;;\334"
  "\334\334\234\360\360\360\4\334\334\334;;;:::\333\333\333\234\360\360"
  "\360\5\333\333\333:::999\330\330\330\356\356\356\232\357\357\357\6\356"
  "\356\356\330\330\330999777\271\271\271\330\327\327\232\330\330\330\5"
  "\330\327\330\271\271\271777888555\234888\2""555888"};


static const guint8 icoQuad[] __attribute__ ((__aligned__ (4))) =
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (927) */
  "\0\0\3\267"
  /* pixdata_type (0x2010001) */
  "\2\1\0\1"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (32) */
  "\0\0\0\40"
  /* height (32) */
  "\0\0\0\40"
  /* pixel_data: */
  "\2PPPMMM\234PPP\4MMMPPPMMM\330\330\330\215\354\354\354\2bbb\337\337\337"
  "\215\354\354\354\4\330\330\330MMMNNN\353\353\353\215\372\372\372\2bb"
  "b\337\337\337\215\372\372\372\4\353\353\353NNNMMN\353\353\353\215\372"
  "\372\372\2bbb\337\337\337\215\372\372\372\1\353\353\353\202MMM\1\353"
  "\353\353\215\372\372\372\2bbb\337\337\337\215\372\372\372\4\353\353\353"
  "MMMLLL\352\352\352\215\372\372\372\2bbb\337\337\337\215\372\372\372\5"
  "\352\352\352LLLKKK\351\351\351\371\371\371\214\372\372\372\2bbb\337\337"
  "\337\214\372\372\372\5\371\371\371\351\351\351KKKJJJ\351\351\351\215"
  "\371\371\371\2bbb\337\337\337\215\371\371\371\5\351\351\351JJJJII\351"
  "\350\350\370\370\370\214\371\371\371\2bbb\337\337\337\214\371\371\371"
  "\5\370\370\370\351\350\351JJIIII\350\350\350\215\370\370\370\2bbb\337"
  "\337\337\215\370\370\370\4\350\350\350IIIHHH\347\347\347\215\370\370"
  "\370\2bbb\337\337\337\215\370\370\370\5\347\347\347HHHGGG\346\346\346"
  "\367\367\367\214\370\370\370\2bbb\337\337\337\214\370\370\370\5\367\367"
  "\367\346\346\346GGGFFF\345\345\345\215\367\367\367\2bbb\337\337\337\215"
  "\367\367\367\1\345\345\345\202FFF\2\345\345\345\367\366\366\214\367\367"
  "\367\2bbb\337\337\337\214\367\367\367\6\367\367\366\345\345\345FFFEE"
  "E\345\345\345\366\366\366\214\367\367\367\2bbb\337\337\337\214\367\367"
  "\367\4\366\366\366\345\345\345EEEDDD\236bbb\2DDDCCC\216\337\337\337\1"
  "bbb\216\337\337\337\5\343\343\343CCCBBC\342\343\342\364\364\364\214\365"
  "\365\365\2bbb\337\337\337\214\365\365\365\5\364\364\364\343\343\342C"
  "CCBBB\342\342\342\215\364\364\364\2bbb\337\337\337\215\364\364\364\4"
  "\342\342\342BBBAAA\341\341\341\215\364\364\364\2bbb\337\337\337\215\364"
  "\364\364\5\341\341\341AAA@@@\340\340\340\363\363\363\214\364\364\364"
  "\2bbb\337\337\337\214\364\364\364\5\363\363\363\340\340\340@@@\77\77"
  "\77\337\337\337\215\363\363\363\2bbb\337\337\337\215\363\363\363\1\337"
  "\337\337\202\77\77\77\2\337\337\337\362\362\362\214\363\363\363\2bbb"
  "\337\337\337\214\363\363\363\6\362\362\363\337\337\337\77\77\77>>>\336"
  "\336\336\362\362\362\214\363\363\363\2bbb\337\337\337\214\363\363\363"
  "\5\362\362\362\336\336\336>>>===\336\336\336\215\362\362\362\2bbb\337"
  "\337\337\215\362\362\362\5\336\336\336===<<<\335\335\335\361\361\361"
  "\214\362\362\362\2bbb\337\337\337\214\362\362\362\6\361\361\361\335\335"
  "\335<<<;;;\335\334\334\360\360\360\214\361\361\361\2bbb\337\337\337\214"
  "\361\361\361\5\360\360\360\335\335\335<<;;;;\334\334\334\215\360\360"
  "\360\2bbb\337\337\337\215\360\360\360\4\334\334\334;;;:::\333\333\333"
  "\215\360\360\360\2bbb\337\337\337\215\360\360\360\5\333\333\333:::99"
  "9\330\330\330\356\356\356\214\357\357\357\2bbb\337\337\337\214\357\357"
  "\357\6\356\356\356\330\330\330999777\271\271\271\330\327\327\214\330"
  "\330\330\2bbb\337\337\337\214\330\330\330\5\330\327\330\271\271\2717"
  "77888555\234888\2""555888"};


