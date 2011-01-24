#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

static void *img_expand24(int width, int height, int stride, const void *bytes)
{
    uint8_t *rgb = malloc(width*height*3);
    if(rgb == NULL) return NULL;

    int adjust = stride - 3*width;
    const uint8_t *p = bytes;
    uint8_t *q = rgb;

    while(p < (const uint8_t *)bytes + width*height) {
        const uint8_t *end = p + width;
        while(p < end) {
            *q++ = *p;
            *q++ = *p;
            *q++ = *p;
            p++;
        }
        q += adjust;
    }

    return rgb;
}

static void *img_expand32(int width, int height, int stride, const void *bytes)
{
    uint32_t *rgb = malloc(width*height*4);
    if(rgb == NULL) return NULL;

    int adjust = stride - 4*width;
    const uint8_t *p = bytes;
    uint32_t *q = rgb;

    while(p < (const uint8_t *)bytes + width*height) {
        const uint8_t *end = p + width;
        while(p < end)
            *q++ = 0x010101 * (*p++);
        q = (uint32_t *)((uint8_t *)q + adjust);
    }

    return rgb;
}

static void *img_collapse24(int width, int height, int stride, const void *rgb)
{
    uint8_t *bytes = malloc(width*height);
    if(bytes == NULL) return NULL;

    int adjust = stride - 3*width;
    uint8_t *p = bytes;
    const uint8_t *q = rgb;

    while(p < bytes + width*height) {
        const uint8_t *end = p + width;
        while(p < end) {
            *p++ = *q;
            q += 3;
        }
        q += adjust;
    }

    return bytes;
}




int img_read(const char *filename, int *widthp, int *heightp, uint8_t **bytesp)
{
    GdkPixbuf *pb =
        gdk_pixbuf_new_from_file(filename, NULL);
    if(pb == NULL)
        return -1;

    if(gdk_pixbuf_get_n_channels(pb) != 3 ||
       gdk_pixbuf_get_bits_per_sample(pb) != 8 ||
       gdk_pixbuf_get_has_alpha(pb) != FALSE) {
        g_object_unref(G_OBJECT(pb));
        return -1;
    }

    uint8_t *data = gdk_pixbuf_get_pixels(pb);
    int width     = gdk_pixbuf_get_width(pb),
        height    = gdk_pixbuf_get_height(pb),
        stride    = gdk_pixbuf_get_rowstride(pb);

    uint8_t *bytes = img_collapse24(width, height, stride, data);
    if(bytes == NULL) {
        g_object_unref(G_OBJECT(pb));
        return -1;
    }

    g_object_unref(G_OBJECT(pb));
    *widthp = width;
    *heightp = height;
    *bytesp = bytes;
    return 0;
}

int img_write(const char *filename, int width, int height, const uint8_t *bytes)
{
    int fnamelen = strlen(filename);
    char *type;

    if(fnamelen>=4 && strcmp(".pgm", filename+fnamelen-4) == 0)
        type = "pgm";
    else if(fnamelen>=4 && strcmp(".png", filename+fnamelen-4) == 0)
        type = "png";
    else if(fnamelen>=4 && strcmp(".jpg", filename+fnamelen-4) == 0)
        type = "jpeg";
    else
        return -1;

    void *rgb = img_expand24(width, height, 3*width, bytes);
    if(rgb == NULL)
        return -1;

    GdkPixbuf *pb =
        gdk_pixbuf_new_from_data(rgb, GDK_COLORSPACE_RGB, FALSE, 8,
                                 width, height, 3*width, NULL,NULL);
    if(!pb) {
        free(rgb);
        return -1;
    }

    if(!gdk_pixbuf_save(pb, filename, type, NULL, NULL)) {
        g_object_unref(G_OBJECT(pb));
        free(rgb);
        return -1;
    }

    g_object_unref(G_OBJECT(pb));
    free(rgb);
    return 0;
}

static inline int mini(int a, int b) { return a < b ? a : b; }
uint32_t img_checksum(int width, int height, const uint8_t *bytes)
{
    uint32_t sum1 = 0xffff, sum2 = 0xffff;

    int len = width*height/2;
    const uint16_t *ptr = (const uint16_t *)bytes;

    while (len) {
        int block = mini(360, len);
        len -= block;
        while(block--) 
            sum1 += *ptr++,
            sum2 += sum1;
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    
    if(width*height%2 == 1)
        sum1 += bytes[width*height-1], sum2 += sum1;
    
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    
    return sum2 << 16 | sum1;
}

static cairo_user_data_key_t img_free_cairo_buffer_key;
cairo_surface_t *img_makesurface(int width, int height, const uint8_t *bytes)
{
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);

    unsigned char *data = img_expand32(width, height, stride, bytes);
    if(data == NULL)
        return NULL;

    cairo_surface_t *surface = 
        cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, width, height, stride);

    if(cairo_surface_status(surface) == CAIRO_STATUS_NO_MEMORY) {
        free(data);
        return NULL;
    }

    cairo_surface_set_user_data(surface, &img_free_cairo_buffer_key, data, free);

    return surface;
}
