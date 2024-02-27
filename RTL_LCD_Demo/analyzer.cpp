/*
 *  analyzer.c
 *
 *  Created on: Feb 27, 2024
 */

/******************************************************************************/
/*                              INCLUDE FILES                                 */
/******************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "analyzer.h"

/******************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                         */
/******************************************************************************/

#define RSSI_FLOOR   (-100)

/******************************************************************************/
/*                              PRIVATE DATA                                  */
/******************************************************************************/

/* Channel legend mapping */
static uint16_t channel_legend[] = {
    1, 2, 3, 4, 5, 6, 7,      /*   1,   2,   3,   4,   5,   6,   7, */
    8, 9, 10, 11, 12, 13, 14, /*   8,   9,  10,  11,  12,  13,  14, */
    32, 0, 0, 0, 40, 0, 0,    /*  32,  34,  36,  38,  40,  42,  44, */
    0, 48, 0, 0, 0, 56, 0,    /*  46,  48,  50,  52,  54,  56,  58, */
    0, 0, 64, 0, 0, 0,        /*  60,  62,  64,  68, N/A,  96,      */
    100, 0, 0, 0, 108, 0, 0,  /* 100, 102, 104, 106, 108, 110, 112, */
    0, 116, 0, 0, 0, 124, 0,  /* 114, 116, 118, 120, 122, 124, 126, */
    0, 0, 132, 0, 0, 0, 140,  /* 128, N/A, 132, 134, 136, 138, 140, */
    0, 0, 0, 149, 0, 0, 0,    /* 142, 144, N/A, 149, 151, 153, 155, */
    157, 0, 0, 0, 165, 0, 0,  /* 157, 159, 161, 163, 165, 167, 169, */
    0, 173
}; 

/* Channel color mapping */
static uint16_t channel_color[] = {
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN,           TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE, TFT_BROWN, TFT_GREEN, TFT_DARKCYAN, TFT_BLUE, TFT_MAGENTA,
    TFT_RED, TFT_ORANGE
};

static uint8_t channel_displayed[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/******************************************************************************/
/*                              EXPORTED DATA                                 */
/******************************************************************************/



/******************************************************************************/
/*                                FUNCTIONS                                   */
/******************************************************************************/



/******************************************************************************/

/**
 * @brief  Return RGB565 color
 */
uint16_t convert_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    uint16_t color565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
    return color565;
}

/*!
 * @brief  Get channel index
 */
static uint16_t channel_index(int channel) {
    if (channel <= 14) {
        return channel - 1;                   /* 2.4 GHz, channel 1-14 */
    }
    if (channel <= 64) {
        return 14 + ((channel - 32) / 2);     /* 5 GHz, channel 32 - 64 */
    }
    if (channel == 68) {
        return 31;
    }
    if (channel == 96) {
        return 33;
    }
    if (channel <= 144) {
        return 34 + ((channel - 100) / 2);    /* channel 98 - 144 */
    }

    return 58 + ((channel - 149) / 2);        /* channel 149 - 177 */
}

/*!
 * @brief  Draw half of ellipse
 */
static void draw_half_ellipse(TFT_eSPI &tft, int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    if (rx<2) return;
    if (ry<2) return;
    int32_t x, y;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t fx2 = 4 * rx2;
    int32_t fy2 = 4 * ry2;
    int32_t s;

    for (x = 0, y = ry, s = 2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++) {
        tft.drawPixel(x0 + x, y0 - y, color);
        tft.drawPixel(x0 - x, y0 - y, color);
        if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
        }
        s += ry2 * ((4 * x) + 6);
    }

    for (x = rx, y = 0, s = 2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++) {
        tft.drawPixel(x0 + x, y0 - y, color);
        tft.drawPixel(x0 - x, y0 - y, color);
        if (s >= 0)
        {
            s += fy2 * (1 - x);
            x--;
        }
        s += rx2 * ((4 * y) + 6);
    }
}

/*!
 * @brief  Draw string info
 */
static void draw_string_info(TFT_eSPI &tft, char *string, int32_t poX, int32_t poY, uint32_t color) {
    tft.setTextDatum(CC_DATUM);
    tft.setTextColor(color);
    tft.drawString(string, poX, poY, 2);
}

/*!
 * @brief  Draw signal info
 */
void analyzer_draw_signal(TFT_eSPI &tft, uint8_t channel, int8_t rssi, char *mac) {
    int idx = channel_index(channel);
    char buf[64];

    int ry = rssi - RSSI_FLOOR;
    sprintf(buf, "%s (%d)", mac, rssi);

    if (channel > 14) {
        draw_half_ellipse(tft, 40 + (idx - 14) * 7, 290, ry / 2, ry, channel_color[idx]);

        if (channel_displayed[idx] == 0) {
            draw_string_info(tft, buf, 40 + (idx - 14) * 7, 280 - ry, channel_color[idx]);
        }
    }
    else {
        draw_half_ellipse(tft, 40 + idx * 30, 170, ry / 2, ry, channel_color[idx]);

        if (channel_displayed[idx] == 0) {
            draw_string_info(tft, buf, 40 + idx * 30, 160 - ry, channel_color[idx]);
        }
    }
    channel_displayed[idx] = 1;
}

/*!
 * @brief  Draw graph
 */
void analyzer_draw_graph(TFT_eSPI &tft) {
    int idx;
    uint8_t channel;
    char buf[16];
    memset(channel_displayed, 0, sizeof(channel_displayed));
    tft.fillRect(0, 40, 480, 320, TFT_WHITE);

    tft.drawLine(20, 170, 460, 170, TFT_COLOR(0x4A6378));
    tft.drawLine(20, 170, 460, 170, TFT_COLOR(0x4A6378));

    tft.drawLine(20, 290, 460, 290, TFT_COLOR(0x4A6378));
    tft.drawLine(20, 290, 460, 290, TFT_COLOR(0x4A6378));

    for (idx = 0; idx < 14; idx++) {
        channel = channel_legend[idx];

        if (channel > 0) {
            sprintf(buf, "%d", channel);
            draw_string_info(tft, buf, 40 + idx * 30, 180, channel_color[idx]);
        }
    }

    for (idx = 14; idx < 71; idx++) {
        channel = channel_legend[idx];
        if (channel > 0) {
            sprintf(buf, "%d", channel);
            draw_string_info(tft, buf, 40 + (idx - 14) * 7, 300, channel_color[idx]);
        }
    }
}
