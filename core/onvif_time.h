/*
 * onvif-c — SetSystemDateAndTime / SetNTP request parsing (issue #22).
 *
 * Pure C, host-testable: turns the SOAP request bodies into plain
 * structs for the onvif_c_config_t time hooks. Local-name matching —
 * namespace prefixes (tds:, tt:, none) are irrelevant to the parse.
 */
#ifndef ONVIF_C_TIME_H
#define ONVIF_C_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** UTC date/time as carried by a SetSystemDateAndTime request. Shared
 * with the public API via include/onvif_c.h (which includes this
 * header) — core stays free of ESP-IDF includes. */
typedef struct {
    uint16_t year;   /* 0 when the request carried no UTCDateTime */
    uint8_t  month;  /* 1..12 */
    uint8_t  day;    /* 1..31 */
    uint8_t  hour;   /* 0..23 */
    uint8_t  minute; /* 0..59 */
    uint8_t  second; /* 0..60 (61 = leap second) */
} onvif_c_utc_time_t;

/** Parsed SetSystemDateAndTime body. */
typedef struct {
    bool have_type; /* body carried a DateTimeType element        */
    bool manual;    /* DateTimeType: true=Manual, false=NTP       */
    bool daylight_savings;
    char tz[64];    /* TimeZone/TZ POSIX string; "" when absent   */
    bool have_utc;  /* body carried a UTCDateTime element         */
    onvif_c_utc_time_t utc; /* zeroed when have_utc is false       */
} onvif_time_set_req_t;

/** Parsed SetNTP body (first NTPServer entry). */
typedef struct {
    bool have_from_dhcp;
    bool from_dhcp;
    bool have_server; /* at least one NTPServer element present    */
    char server[256]; /* first server: DNS name or IP literal      */
} onvif_time_ntp_req_t;

/*
 * Parse a SetSystemDateAndTime SOAP body. Returns false when the body
 * carries none of the expected elements (caller treats it as malformed).
 */
bool onvif_time_parse_set_system_date_and_time(const char *body, onvif_time_set_req_t *out);

/*
 * Parse a SetNTP SOAP body. Returns false when the body carries neither
 * a FromDHCP element nor an NTPServer element.
 */
bool onvif_time_parse_set_ntp(const char *body, onvif_time_ntp_req_t *out);

#endif /* ONVIF_C_TIME_H */
