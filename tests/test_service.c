/*
 * Port-layer tests — SOAP Device/Media dispatch, config seam, lifecycle.
 *
 * Drives the real handlers registered by onvif_c_start() through the fake
 * httpd: request bodies go in via httpd_req_recv, responses are captured
 * per request. The fake clock makes GetSystemDateAndTime deterministic.
 *
 * Discipline: every phase that starts the library ends with stop_env(),
 * because a successful onvif_c_start() leaves the discovery task running.
 */

#include "../include/onvif_c.h"
#include "../esp_idf/onvif_c_port.h"
#include "test_util.h"
#include "onvif_fake.h"
#include <stdlib.h>

/* ---- controllable callbacks ---- */

static int g_gate    = 1;
static int g_fps     = 12;
static int g_ip_mode = 0;  /* 0 good, 1 zero-always */
static int g_ip_zero_once; /* next call returns 0.0.0.0, then re-arms */

static const char *cb_serial(void)
{
    return T_SERIAL;
}
static const char *cb_uuid(void)
{
    return T_UUID;
}

static const char *cb_ip(void)
{
    if (g_ip_mode == 1) {
        return "0.0.0.0";
    }
    if (g_ip_zero_once) {
        g_ip_zero_once = 0;
        return "0.0.0.0";
    }
    return T_IP;
}

static const char *cb_stream_uri(void)
{
    static char uri[64];
    snprintf(uri, sizeof(uri), "rtsp://%s:554/stream", T_IP);
    return uri;
}

static const char *cb_snapshot_uri(void)
{
    return "http://" T_IP "/cap";
}

/* #6 fixtures: integrator strings long enough to overflow a 4096-byte
 * response buffer (would-be length >> actual bytes). */
static const char *cb_stream_uri_huge(void)
{
    static char uri[5000];
    memset(uri, 'u', sizeof(uri) - 1);
    uri[sizeof(uri) - 1] = '\0';
    return uri;
}

static const char *cb_snapshot_uri_huge(void)
{
    return cb_stream_uri_huge();
}
static uint8_t cb_frame_rate(void)
{
    return (uint8_t)g_fps;
}
static bool cb_gate(void)
{
    return g_gate != 0;
}

static onvif_c_config_t base_cfg(void)
{
    onvif_c_config_t c = {0};
    c.serial           = cb_serial;
    c.uuid             = cb_uuid;
    c.ip               = cb_ip;
    c.stream_uri       = cb_stream_uri;
    c.frame_rate       = cb_frame_rate;
    c.http_port        = 80;
    return c;
}

static void reset_env(void)
{
    onvif_fake_httpd_reset();
    onvif_fake_net_reset();
    onvif_fake_time_set(T_EPOCH);
    g_gate         = 1;
    g_fps          = 12;
    g_ip_mode      = 0;
    g_ip_zero_once = 0;
}

static void stop_env(void)
{
    onvif_c_stop();
    onvif_fake_task_drain();
}

static char g_body[512];

/* POST one SOAP action; body and length are built in a fixed order. */
static esp_err_t post(httpd_uri_t *u, const char *action, httpd_req_t *r)
{
    snprintf(g_body, sizeof(g_body),
             "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
             "<s:Body>%s</s:Body></s:Envelope>",
             action);
    return onvif_fake_httpd_invoke(u, g_body, strlen(g_body), r);
}

void test_service(void)
{
    httpd_req_t      r;
    httpd_uri_t     *u;
    onvif_c_config_t cfg;
    httpd_handle_t   hd = onvif_fake_httpd_handle();

    /* ---- onvif_c_start argument validation ---- */
    reset_env();
    cfg = base_cfg();
    CHECK(onvif_c_start(NULL, &cfg) == ESP_ERR_INVALID_ARG, "NULL httpd rejected");
    CHECK(onvif_c_start(hd, NULL) == ESP_ERR_INVALID_ARG, "NULL cfg rejected");
    cfg        = base_cfg();
    cfg.serial = NULL;
    CHECK(onvif_c_start(hd, &cfg) == ESP_ERR_INVALID_ARG, "NULL serial rejected");
    cfg      = base_cfg();
    cfg.uuid = NULL;
    CHECK(onvif_c_start(hd, &cfg) == ESP_ERR_INVALID_ARG, "NULL uuid rejected");
    cfg    = base_cfg();
    cfg.ip = NULL;
    CHECK(onvif_c_start(hd, &cfg) == ESP_ERR_INVALID_ARG, "NULL ip rejected");
    cfg            = base_cfg();
    cfg.stream_uri = NULL;
    CHECK(onvif_c_start(hd, &cfg) == ESP_ERR_INVALID_ARG, "NULL stream_uri rejected");
    CHECK(onvif_fake_httpd_register_count() == 0, "nothing registered on bad args");

    /* ---- happy start, no events ---- */
    reset_env();
    cfg = base_cfg(); /* events_enabled == NULL */
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start OK");
    CHECK(onvif_fake_httpd_register_count() == 2, "device+media registered");
    CHECK(onvif_fake_httpd_find("/onvif/events_service") == NULL,
          "no events handler without events_enabled");
    CHECK(onvif_c_version() == ONVIF_C_VERSION, "version macro exported");
    u = onvif_fake_httpd_find("/onvif/device_service");
    CHECK(u != NULL, "device handler found");

    /* GetSystemDateAndTime — deterministic via the fake clock */
    onvif_fake_time_set(T_EPOCH);
    CHECK(post(u, "<tds:GetSystemDateAndTime/>", &r) == ESP_OK, "date handler ok");
    CHECK_SUB(r.resp, "<tds:GetSystemDateAndTimeResponse>", "date response tag");
    CHECK_SUB(r.resp, "<tt:TZ>UTC</tt:TZ>", "date timezone");
    {
        time_t    t = T_EPOCH;
        struct tm tm;
        char      exp[40];
        gmtime_r(&t, &tm);
        snprintf(exp, sizeof(exp), "<tt:Hour>%d</tt:Hour>", tm.tm_hour);
        CHECK_SUB(r.resp, exp, "date hour from fake clock");
    }

    /* GetDeviceInformation — defaults resolve for every optional field */
    cfg = base_cfg(); /* manufacturer/model/hw/fw all NULL */
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "restart with new cfg ok");
    CHECK(post(u, "<tds:GetDeviceInformation/>", &r) == ESP_OK, "info handler ok");
    CHECK_SUB(r.resp, "<tds:Manufacturer>MiBee</tds:Manufacturer>", "default manufacturer");
    CHECK_SUB(r.resp, "<tds:Model>MiBeeCam</tds:Model>", "default model");
    CHECK_SUB(r.resp, "<tds:HardwareId>ESP32</tds:HardwareId>", "default hardware id");
    CHECK_SUB(r.resp, "<tds:FirmwareVersion>v0.1.0</tds:FirmwareVersion>",
              "default firmware version");
    CHECK_SUB(r.resp, "<tds:SerialNumber>" T_SERIAL "</tds:SerialNumber>", "serial from callback");
    CHECK_SUB(r.resp, "GetDeviceInformationResponse", "info response tag");

    /* default WS-Discovery scopes built from the model */
    CHECK_SUB(onvif_c_cfg_scopes(), "onvif://www.onvif.org/name/MiBeeCam",
              "default scopes include model name");
    CHECK(!onvif_c_cfg_has_events(), "has_events false without callback");

    /* ---- capabilities variants (http_port plumbing end to end) ---- */
    cfg           = base_cfg();
    cfg.http_port = 8080;
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with port 8080");
    CHECK(post(u, "<tds:GetCapabilities/>", &r) == ESP_OK, "caps handler ok");
    CHECK_SUB(r.resp, "http://" T_IP ":8080/onvif/device_service",
              "caps device XAddr honors http_port");
    CHECK_SUB(r.resp, "http://" T_IP ":8080/onvif/media_service",
              "caps media XAddr honors http_port");
    CHECK(strstr(r.resp, "events_service") == NULL, "no events XAddr without events_enabled");

    cfg                = base_cfg();
    cfg.http_port      = 8080;
    cfg.events_enabled = cb_gate;
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with events");
    CHECK(onvif_fake_httpd_find("/onvif/events_service") != NULL, "events handler registered");
    CHECK(onvif_c_cfg_has_events(), "has_events true with callback");
    CHECK(post(u, "<tds:GetCapabilities/>", &r) == ESP_OK, "caps handler ok");
    CHECK_SUB(r.resp, "http://" T_IP ":8080/onvif/events_service", "events XAddr when enabled");

    cfg                = base_cfg();
    cfg.http_port      = 8080;
    cfg.events_enabled = cb_gate;
    cfg.scopes         = "CUSTOM-SCOPES";
    g_ip_mode          = 1; /* ip callback answers 0.0.0.0 */
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with placeholder ip");
    CHECK(post(u, "<tds:GetCapabilities/>", &r) == ESP_OK, "caps handler ok");
    CHECK_SUB(r.resp, "http://0.0.0.0:8080/onvif/device_service", "placeholder ip propagated");
    CHECK(strcmp(onvif_c_cfg_scopes(), "CUSTOM-SCOPES") == 0, "custom scopes override default");
    g_ip_mode = 0;
    stop_env();

    /* ---- media service ---- */
    reset_env();
    cfg            = base_cfg();
    cfg.frame_rate = NULL; /* exercise the documented default (15) */
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "media start ok");
    u = onvif_fake_httpd_find("/onvif/media_service");
    CHECK(u != NULL, "media handler found");

    CHECK(post(u, "<trt:GetProfiles/>", &r) == ESP_OK, "profiles handler ok (default fps)");
    CHECK_SUB(r.resp, "<tt:FrameRateLimit>15</tt:FrameRateLimit>", "default frame rate 15");
    cfg   = base_cfg(); /* frame_rate callback present */
    g_fps = 12;
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "restart with frame_rate cb");
    CHECK(post(u, "<trt:GetProfiles/>", &r) == ESP_OK, "profiles handler ok (cb fps)");
    CHECK_SUB(r.resp, "<tt:FrameRateLimit>12</tt:FrameRateLimit>", "frame rate from callback");

    CHECK(post(u, "<trt:GetStreamUri/>", &r) == ESP_OK, "stream uri handler ok");
    CHECK_SUB(r.resp, "<tt:Uri>rtsp://" T_IP ":554/stream</tt:Uri>", "stream uri from callback");

    /* GetSnapshotUri: derived URI follows http_port */
    cfg           = base_cfg();
    cfg.http_port = 8080; /* snapshot_uri NULL -> derived */
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start for derived snapshot");
    CHECK(post(u, "<trt:GetSnapshotUri/>", &r) == ESP_OK, "snapshot handler ok");
    CHECK_SUB(r.resp, "http://" T_IP ":8080/api/capture", "derived snapshot uri honors http_port");
    CHECK(post(u, "<trt:GetSnapshot/>", &r) == ESP_OK, "bare GetSnapshot ok");
    CHECK_SUB(r.resp, "http://" T_IP ":8080/api/capture", "GetSnapshot alias dispatches");

    cfg              = base_cfg();
    cfg.snapshot_uri = cb_snapshot_uri;
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with snapshot callback");
    CHECK(post(u, "<trt:GetSnapshotUri/>", &r) == ESP_OK, "snapshot handler ok");
    CHECK_SUB(r.resp, "<tt:Uri>http://" T_IP "/cap</tt:Uri>", "explicit snapshot uri wins");

    /* ---- faults and body edge cases ---- */
    CHECK(post(u, "<trt:GetVideoEncoderConfigurations/>", &r) == ESP_OK,
          "unknown media action handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "unknown media action fault");

    CHECK(onvif_fake_httpd_invoke(u, NULL, 0, &r) == ESP_OK, "empty body handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "empty body fault");

    CHECK(onvif_fake_httpd_invoke(u, "x", 5000, &r) == ESP_OK, "oversize body handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "oversize body fault");

    onvif_fake_httpd_recv_fail_once();
    CHECK(post(u, "<trt:GetProfiles/>", &r) == ESP_OK, "recv failure handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "recv failure fault");

    /* a short first recv (one TCP segment) must be reassembled across
     * recv() calls, not degrade to a fault (#7) */
    u = onvif_fake_httpd_find("/onvif/device_service");
    onvif_fake_httpd_recv_short_once(8);
    CHECK(post(u, "<tds:GetCapabilities/>", &r) == ESP_OK, "partial read handled");
    CHECK_SUB(r.resp, "GetCapabilitiesResponse", "partial body reassembled");

    CHECK(onvif_fake_httpd_invoke(u, "zzz", 3, &r) == ESP_OK, "garbage body handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "garbage body fault");

    CHECK(post(u, "<tds:GetUsers/>", &r) == ESP_OK, "unknown device action handled");
    CHECK_SUB(r.resp, "ter:ActionNotSupported", "unknown device action fault");

    /* oversized integrator strings would overflow the response buffer: the
     * builder's would-be length is NOT a byte count — must fault, never
     * transmit past the buffer (#6; would be an ASan heap over-read) */
    {
        static char big[5000];
        memset(big, 'M', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        cfg                  = base_cfg();
        cfg.model            = big;
        CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with oversized model");
        CHECK(post(u, "<tds:GetDeviceInformation/>", &r) == ESP_OK, "oversized model handled");
        CHECK_SUB(r.resp, "ter:ActionNotSupported",
                  "oversized model faults instead of over-reading");
        CHECK(r.resp_len < FAKE_HTTPD_RESP_MAX, "response stays bounded");
    }
    {
        cfg              = base_cfg();
        cfg.stream_uri   = cb_stream_uri_huge;
        cfg.snapshot_uri = cb_snapshot_uri_huge;
        CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "start with oversized uris");
        u = onvif_fake_httpd_find("/onvif/media_service");
        CHECK(post(u, "<trt:GetStreamUri/>", &r) == ESP_OK, "oversized stream uri handled");
        CHECK_SUB(r.resp, "ter:ActionNotSupported",
                  "oversized stream uri faults instead of over-reading");
        CHECK(post(u, "<trt:GetSnapshotUri/>", &r) == ESP_OK, "oversized snapshot uri handled");
        CHECK_SUB(r.resp, "ter:ActionNotSupported",
                  "oversized snapshot uri faults instead of over-reading");
    }
    stop_env();

    /* ---- registration failure paths ---- */
    reset_env();
    cfg = base_cfg();
    onvif_fake_httpd_fail_register_once(ESP_ERR_HTTPD_HANDLER_EXISTS);
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "HANDLER_EXISTS on first uri tolerated");
    stop_env();

    reset_env();
    cfg = base_cfg();
    onvif_fake_httpd_fail_register_once(ESP_ERR_NO_MEM);
    CHECK(onvif_c_start(hd, &cfg) == ESP_ERR_NO_MEM, "hard registration failure propagates");
    CHECK(onvif_fake_httpd_register_count() == 0, "failed register stored nothing");

    reset_env();
    cfg                = base_cfg();
    cfg.events_enabled = cb_gate;
    onvif_fake_httpd_fail_register_nth(3, ESP_ERR_HTTPD_HANDLER_EXISTS);
    CHECK(onvif_c_start(hd, &cfg) == ESP_OK, "events HANDLER_EXISTS tolerated");
    stop_env();

    reset_env();
    cfg = base_cfg();
    onvif_fake_task_fail_create_once();
    CHECK(onvif_c_start(hd, &cfg) == ESP_FAIL, "discovery task creation failure propagates");
    CHECK(onvif_fake_httpd_register_count() == 2, "handlers stay registered when discovery fails");
}
