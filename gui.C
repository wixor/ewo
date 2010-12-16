#include <cstdio>
#include <cstring>
#include <stdarg.h>

#include "image.h"
#include "composite.h"
#include "gui.h"

DisplaySlot::DisplaySlot(const char *initname)
{
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    events = 0;
    name = strdup(initname);
    caption = strdup("");
    active = false;

    img.resize(100,100);
    cr_surface = img.getCairoSurface();
}

void DisplaySlot::activate()
{
    if(active)
        return;

    gui_gtk_register(this);

    pthread_mutex_lock(&lock);
    sendEventSync(DS_INIT);
    pthread_mutex_unlock(&lock);

    active = true;
}

DisplaySlot::~DisplaySlot()
{
    if(active) {
        pthread_mutex_lock(&lock);
        sendEventSync(DS_CLEANUP);
        pthread_mutex_unlock(&lock);

        gui_gtk_unregister(this);
    }

    free(caption);
    free(name);

    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&lock);
}

void DisplaySlot::sendEvent(int ev) {
    events |= ev;
    gui_gtk_poke();
}
void DisplaySlot::sendEventSync(int ev) {
    sendEvent(ev);
    pthread_cond_wait(&cond, &lock);
}

void DisplaySlot::rename(const char *fmt, ...)
{
    char *buf;
    va_list args;
    va_start(args, fmt);
    vasprintf(&buf, fmt, args);
    va_end(args);

    activate();
    pthread_mutex_lock(&lock);
    free(name);
    name = buf;
    sendEvent(DS_RENAME);
    pthread_mutex_unlock(&lock);
}

void DisplaySlot::recaption(const char *fmt, ...)
{
    char *buf;
    va_list args;
    va_start(args, fmt);
    vasprintf(&buf, fmt, args);
    va_end(args);

    activate();
    pthread_mutex_lock(&lock);
    free(caption);
    caption = buf;
    sendEvent(DS_RECAPTION);
    pthread_mutex_unlock(&lock);
}

void DisplaySlot::update(const CairoImage &src)
{
    activate();
    getCanvas();
    img = src;
    putCanvas();
}

void DisplaySlot::update(const Image &src, bool transparency)
{
    activate();
    getCanvas();
    img.fromImage(src, transparency);
    putCanvas();
}

void DisplaySlot::bind()
{
    activate();
    pthread_mutex_lock(&lock);
    sendEvent(DS_BIND);
    pthread_mutex_unlock(&lock);
}

void DisplaySlot::unbind()
{
    activate();
    pthread_mutex_lock(&lock);
    sendEvent(DS_UNBIND);
    pthread_mutex_unlock(&lock);
}

CairoImage *DisplaySlot::getCanvas()
{
    activate();
    pthread_mutex_lock(&lock);
    return &img;
}
void DisplaySlot::putCanvas()
{
    cr_surface = img.getCairoSurface();
    sendEvent(DS_UPDATE);
    pthread_mutex_unlock(&lock);
}
