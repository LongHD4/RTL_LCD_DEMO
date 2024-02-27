/*
 *  main.cpp
 *
 *  Created on: Jan 5, 2024
 */

/******************************************************************************/

/******************************************************************************/
/*                              INCLUDE FILES                                 */
/******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <wifi_conf.h>
#include <TFT_eSPI.h>
#include "analyzer.h"

/******************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                         */
/******************************************************************************/

#define WIFI_SCANNING_TIME_S 15    /* Scan network for 10 seconds */
#define INVALID_RSSI (-128)

#define MAX_WIFI_SIGNAL 32
typedef struct {
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
    int count;
    uint32_t lastseen;
} wifi_signal_t;

typedef struct {
    wifi_signal_t signals[MAX_WIFI_SIGNAL];
    uint8_t count;
} wifi_list_t;

/******************************************************************************/
/*                              PRIVATE DATA                                  */
/******************************************************************************/

static TFT_eSPI tft = TFT_eSPI();
static wifi_list_t wifi_signals;

/******************************************************************************/
/*                              EXPORTED DATA                                 */
/******************************************************************************/



/******************************************************************************/
/*                                FUNCTIONS                                   */
/******************************************************************************/



/******************************************************************************/

/**
 * @brief  Draw a string
 */
static void draw_string_color(const char *string, int32_t poX, int32_t poY, uint8_t font = 2, uint32_t color = TFT_BLACK, uint8_t d = CC_DATUM) {
    tft.setTextDatum(d);
    tft.setTextColor(color);
    tft.drawString(string, poX, poY, font);
}

/**
 * @brief  Initialize LCD for gui
 */
static void lcd_gui_init(void) {
    tft.init();
    tft.setRotation(3);
    tft.fillRect(0, 0, 480, 40, TFT_COLOR(0x93C1EB));
    draw_string_color("WiFi Analyzer Demo", 240, 20, 4, TFT_WHITE);
}

/*!
 * @brief  Display analysis
 */
static void main_update_analysis(void) {
    char mac_str[16];
    analyzer_draw_graph(tft);
    
    for (int i = 0; i < wifi_signals.count; i++) {
        sprintf(mac_str, "%02X%02X%02X%02X%02X%02X", wifi_signals.signals[i].bssid[0], wifi_signals.signals[i].bssid[1],
                wifi_signals.signals[i].bssid[2], wifi_signals.signals[i].bssid[3], wifi_signals.signals[i].bssid[4], wifi_signals.signals[i].bssid[5]);
        analyzer_draw_signal(tft, wifi_signals.signals[i].channel, wifi_signals.signals[i].rssi, mac_str);
    }
}

/*!
 * @brief  Scan networks results callback
 */
static rtw_result_t scan_result_handler(rtw_scan_handler_result_t* malloced_scan_result) {
    int i;

    if (malloced_scan_result->scan_complete != RTW_TRUE) {
        rtw_scan_result_t* record = &malloced_scan_result->ap_details;

        for (i = 0; i < wifi_signals.count; i++) {
            /* If matched with old ap */
            if (!memcmp(record->BSSID.octet, wifi_signals.signals[i].bssid, 6)) {
                break;
            }
        }
        if (i >= wifi_signals.count) {
            wifi_signals.count++;
        }

        if (i < MAX_WIFI_SIGNAL) {
            memcpy(wifi_signals.signals[i].bssid, record->BSSID.octet, 6);    /* Get AP BSSID */
            wifi_signals.signals[i].channel = record->channel;
            wifi_signals.signals[i].rssi = record->signal_strength;
        }
    }
    return RTW_SUCCESS;
}

/*!
 * @brief  Disable wifi
 */
void wifi_idle(void) {
    wifi_on(RTW_MODE_NONE);
    wifi_off();
}

/*!
 * @brief  Scan networks
 */
void scan_networks(int timeout) {
    wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
    wifi_on(RTW_MODE_STA);

    /* Start scanning networks */
    memset(&wifi_signals, 0, sizeof(wifi_signals));
    if (wifi_scan_networks(scan_result_handler, NULL) != RTW_SUCCESS) {
        return;
    }

    /* Wait for scan finished */
    delay(timeout * 1000);
    wifi_idle();
}

/******************************************************************************/

void setup() {
    Serial.begin(115200);
    Serial.println("RTL8720 TFT LCD Demo");

    /* LCD init */
    lcd_gui_init();
}

void loop() {
    main_update_analysis();
    scan_networks(WIFI_SCANNING_TIME_S);    /* Shows analysis 15 seconds before reload */
}
