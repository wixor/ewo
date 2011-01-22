#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

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

    uint8_t *bytes = malloc(width*height);
    if(bytes == NULL) {
        g_object_unref(G_OBJECT(pb));
        return -1;
    }

    {
        uint8_t *dst = bytes, *src = data;
        while(dst != bytes+width*height)
        {
            uint8_t *end = dst + width, *row = src;
            while(dst != end) {
                *dst = src[0];
                dst++;
                src+=3;
            }
            src = row+stride;
        }
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

    /* must expand to 8-bit rgb */
    uint8_t *rgb = malloc(width*height*3);
    if(rgb == NULL) return -1;

    {
        const uint8_t *p = bytes;
        uint8_t *q = rgb;
        while(p <  bytes+width*height) {
            q[0] = q[1] = q[2] = *p;
            p++;
            q+=3;
        }
    }

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

