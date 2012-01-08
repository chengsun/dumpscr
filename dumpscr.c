/*
dumpscr
chengsun.github.org
Public Domain

Requires libpng and xcb
*/
 
#include <xcb/xcb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>

/* TODO: make this config'able */
#define DUMPLOC "dumpscr.png"

static void error_handler(png_structp unused, png_const_charp msg)
{
    fprintf(stderr, "libpng failed: %s\n", msg);
    exit(3);
}

/*
0 = no error
1 = xcb error
2 = out of memory
*/
int dumpscr() {
    int ret = 0;
    xcb_connection_t *connection;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t iter;
    
    unsigned i, count;
    xcb_get_image_cookie_t *cookies;
    unsigned *scr_widths, *scr_heights;
    unsigned img_width = 0, img_height = 0;
    unsigned img_col = 0;
    unsigned row, col;
    
    uint8_t *data;       /* output bitmap */
    FILE *fout;
    
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    
    
    /* send requests */
    
    connection = xcb_connect(NULL, NULL);
    setup = xcb_get_setup(connection);
    iter = xcb_setup_roots_iterator(setup);
    
    count = iter.rem;
    cookies = malloc(count * sizeof(xcb_get_image_cookie_t));
    scr_widths = malloc(count * sizeof(unsigned));
    scr_heights = malloc(count * sizeof(unsigned));

    for (i = 0; i < count; ++i) {
        xcb_screen_t *screen = iter.data;
        
        cookies[i] = xcb_get_image(connection,
                                   XCB_IMAGE_FORMAT_Z_PIXMAP,
                                   screen->root,
                                   0, 0,
                                   screen->width_in_pixels, screen->height_in_pixels,
                                   (uint32_t) -1);     /* << should be able to mask out alpha here? */
        
        scr_widths[i] = screen->width_in_pixels;
        scr_heights[i] = screen->height_in_pixels;
        img_width += screen->width_in_pixels;
        if (img_height < screen->height_in_pixels) {
            img_height = screen->height_in_pixels;
        }

        xcb_screen_next(&iter);
    }


    /* init png stuff */
    
    fout = fopen(DUMPLOC, "wb");
    
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, error_handler, NULL);
    if (!png_ptr) {
        ret = 2;
        goto cleanup;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        ret = 2;
        goto cleanup;
    }

    png_init_io(png_ptr, fout);

    png_set_IHDR(png_ptr, info_ptr, img_width, img_height,
                 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
    
    
    /* read and process replies */
    
    data = malloc(img_width * img_height * 4);
    if (!data) {
        ret = 2;
        goto cleanup;
    }
    
    for (i = 0; i < count; ++i) {
        xcb_generic_error_t *error;
        xcb_get_image_reply_t *reply = xcb_get_image_reply(connection,
                                                           cookies[i],
                                                           &error);
        uint8_t *scr_data;
        
        if (error || !reply) {
            fprintf(stderr, "xcb returned error %d\n", error->error_code);
        
            free(error);
            free(reply);
            
            ret = 1;
            goto cleanup;
        }
        
        scr_data = xcb_get_image_data(reply);
        
        for (row = 0; row < scr_heights[i]; ++row) {
            for (col = 0; col < scr_widths[i]; ++col) {
                unsigned offset = (row * img_width + col + img_col) * 4;
                /* scr_data is in BGR format */
                data[offset + 0] = scr_data[2];
                data[offset + 1] = scr_data[1];
                data[offset + 2] = scr_data[0];
                data[offset + 3] = 0xFF;
                
                scr_data += 4;
            }
        }
        
        /* fill rest with transparency */
        for (; row < img_height; ++row) {
            for (col = 0; col < scr_widths[i]; ++col) {
                unsigned offset = (row * img_width + col + img_col) * 4;
                data[offset + 0] = 0x00;
                data[offset + 1] = 0x00;
                data[offset + 2] = 0x00;
                data[offset + 3] = 0x00;
            }
        }
        
        img_col += scr_widths[i];
        
        free(reply);
    }
    
    
    /* write to png */
    
    for (row = 0; row < img_height; ++row) {
        png_write_row(png_ptr, data + row * img_width * 4);
    }
    
    png_write_end(png_ptr, NULL);
    
    
    /* fin */
    
cleanup:
    if (png_ptr) {
        png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : NULL);
    }
    fclose(fout);
    free(cookies);
    xcb_disconnect(connection);
    return ret;
}
 
int main()
{
    int ret = dumpscr();
    if (ret != 0) {
        fprintf(stderr, "failed to dump screen, error %d\n", ret);
    }
    
    return ret;
}

