/*
 * onvif-c — SetSystemDateAndTime / SetNTP request parsing (issue #22).
 * See onvif_time.h. Extraction is by local name: each element is matched
 * as "<[prefix:]Local ...>text</...>", so client namespace prefixes are
 * irrelevant. Malformed values fall back to zero/false — the caller
 * decides whether that is acceptable for the action.
 */
#include "onvif_time.h"

#include <string.h>

/*
 * Text content of the first element whose local name equals `local`.
 * Returns NULL when absent; "*out_len == 0" marks an empty/self-closing
 * element. Content containing child elements yields its leading text
 * run only (callers here want plain values).
 */
static const char *elem_text(const char *body, const char *local, size_t *out_len)
{
    size_t n = strlen(local);
    const char *p = body;
    while ((p = strchr(p, '<')) != NULL) {
        p++;
        const char *name = p;
        const char *colon = strchr(name, ':');
        if (colon && colon < name + 32 && strchr(name, '>') > colon)
            p = colon + 1;
        if (strncmp(p, local, n) == 0) {
            char c = p[n];
            if (c == '>' || c == ' ' || c == '/' || c == '\t' || c == '\r' || c == '\n') {
                const char *gt = strchr(p + n, '>');
                if (!gt)
                    return NULL;
                if (gt[-1] == '/') { /* self-closing */
                    *out_len = 0;
                    return gt;
                }
                const char *content = gt + 1;
                const char *end = strchr(content, '<');
                if (!end)
                    return NULL;
                *out_len = (size_t)(end - content);
                return content;
            }
        }
        p = name;
    }
    return NULL;
}

/* Pointer past the '>' of the first open tag with local name `local`. */
static const char *elem_open_end(const char *body, const char *local)
{
    size_t n = strlen(local);
    const char *p = body;
    while ((p = strchr(p, '<')) != NULL) {
        p++;
        const char *name = p;
        const char *colon = strchr(name, ':');
        if (colon && colon < name + 32 && strchr(name, '>') > colon)
            p = colon + 1;
        if (strncmp(p, local, n) == 0) {
            char c = p[n];
            if (c == '>' || c == ' ' || c == '/' || c == '\t' || c == '\r' || c == '\n') {
                const char *gt = strchr(p + n, '>');
                return gt ? gt + 1 : NULL;
            }
        }
        p = name;
    }
    return NULL;
}

static int parse_int_text(const char *s, size_t n)
{
    char tmp[16];
    if (n == 0 || n >= sizeof(tmp))
        return -1;
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    int v = 0, neg = 0;
    size_t i = 0;
    if (tmp[0] == '-') {
        neg = 1;
        i = 1;
    }
    for (; i < strlen(tmp); i++) {
        if (tmp[i] < '0' || tmp[i] > '9')
            return -1;
        v = v * 10 + (tmp[i] - '0');
    }
    return neg ? -v : v;
}

static bool parse_bool_text(const char *s, size_t n)
{
    if (n == 4 && (strncmp(s, "true", 4) == 0))
        return true;
    if (n == 1 && s[0] == '1')
        return true;
    return false;
}

static void copy_text(char *dst, size_t dst_n, const char *s, size_t n)
{
    if (n >= dst_n)
        n = dst_n - 1;
    memcpy(dst, s, n);
    dst[n] = '\0';
}

bool onvif_time_parse_set_system_date_and_time(const char *body, onvif_time_set_req_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!body)
        return false;

    size_t n = 0;
    const char *v = elem_text(body, "DateTimeType", &n);
    if (v) {
        out->have_type = true;
        out->manual = (n == 6 && strncmp(v, "Manual", 6) == 0);
    }

    if ((v = elem_text(body, "DaylightSavings", &n)) != NULL)
        out->daylight_savings = parse_bool_text(v, n);

    if ((v = elem_text(body, "TZ", &n)) != NULL && n > 0)
        copy_text(out->tz, sizeof(out->tz), v, n);

    if (elem_open_end(body, "UTCDateTime")) {
        out->have_utc = true;
        if ((v = elem_text(body, "Year", &n)) != NULL)
            out->utc.year = (uint16_t)parse_int_text(v, n);
        if ((v = elem_text(body, "Month", &n)) != NULL)
            out->utc.month = (uint8_t)parse_int_text(v, n);
        if ((v = elem_text(body, "Day", &n)) != NULL)
            out->utc.day = (uint8_t)parse_int_text(v, n);
        if ((v = elem_text(body, "Hour", &n)) != NULL)
            out->utc.hour = (uint8_t)parse_int_text(v, n);
        if ((v = elem_text(body, "Minute", &n)) != NULL)
            out->utc.minute = (uint8_t)parse_int_text(v, n);
        if ((v = elem_text(body, "Second", &n)) != NULL)
            out->utc.second = (uint8_t)parse_int_text(v, n);
    }

    return out->have_type || out->utc.year != 0 || out->tz[0] != '\0';
}

bool onvif_time_parse_set_ntp(const char *body, onvif_time_ntp_req_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!body)
        return false;

    size_t n = 0;
    const char *v = elem_text(body, "FromDHCP", &n);
    if (v) {
        out->have_from_dhcp = true;
        out->from_dhcp = parse_bool_text(v, n);
    }

    /* The first NTPServer's inner token: DNSname / IPv4Address /
     * IPv6Address. The region runs to the next "NTPServer" occurrence
     * (its close tag), so later servers cannot leak in. */
    const char *srv = elem_open_end(body, "NTPServer");
    if (srv) {
        const char *close = strstr(srv, "NTPServer");
        size_t window = close ? (size_t)(close - srv) : strlen(srv);
        char region[512];
        if (window >= sizeof(region))
            window = sizeof(region) - 1;
        memcpy(region, srv, window);
        region[window] = '\0';

        static const char *const kinds[] = {"DNSname", "IPv4Address", "IPv6Address"};
        for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
            if ((v = elem_text(region, kinds[i], &n)) != NULL && n > 0) {
                copy_text(out->server, sizeof(out->server), v, n);
                out->have_server = true;
                break;
            }
        }
    }

    return out->have_from_dhcp || out->have_server;
}
