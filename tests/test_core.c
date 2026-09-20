/*
 * onvif-c host golden tests — pins the byte-stability contract.
 *
 * Plain C99 + system cc, zero dependencies. Every expected string below
 * is the field-proven firmware output; a diff here is a behavior change
 * for NVR integrators, not a cosmetic one.
 */

#include "../core/onvif_xml.h"
#include "../core/onvif_probe.h"
#include "../core/onvif_events_ring.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, name)                                              \
    do {                                                               \
        checks++;                                                      \
        if (!(cond)) {                                                 \
            failures++;                                                \
            printf("FAIL: %s\n", name);                                \
        }                                                              \
    } while (0)

#define CHECK_STR(buf, expected, name)                                 \
    do {                                                               \
        checks++;                                                      \
        if (strcmp(buf, expected) != 0) {                              \
            failures++;                                                \
            printf("FAIL: %s\n  got:  %s\n  want: %s\n",               \
                   name, buf, expected);                               \
        }                                                              \
    } while (0)

static char g[4096];

/* ---- golden strings (extracted from field-proven firmware output) ---- */

static const char *G_SYSTEM_DATE =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:tt=\"http://www.onvif.org/ver10/schema\""
    " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<soap:Body>"
    "<tds:GetSystemDateAndTimeResponse>"
    "<tds:SystemDateAndTime>"
    "<tt:DateTimeType>NTP</tt:DateTimeType>"
    "<tt:DaylightSavings>false</tt:DaylightSavings>"
    "<tt:TimeZone>"
    "<tt:TZ>UTC</tt:TZ>"
    "</tt:TimeZone>"
    "<tt:UTCDateTime>"
    "<tt:Time>"
    "<tt:Hour>7</tt:Hour>"
    "<tt:Minute>42</tt:Minute>"
    "<tt:Second>13</tt:Second>"
    "</tt:Time>"
    "<tt:Date>"
    "<tt:Year>2026</tt:Year>"
    "<tt:Month>9</tt:Month>"
    "<tt:Day>20</tt:Day>"
    "</tt:Date>"
    "</tt:UTCDateTime>"
    "</tds:SystemDateAndTime>"
    "</tds:GetSystemDateAndTimeResponse>"
    "</soap:Body>"
    "</soap:Envelope>";

static const char *G_DEVICE_INFO =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\""
    " xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<soap:Body>"
    "<tds:GetDeviceInformationResponse>"
    "<tds:Manufacturer>MiBee</tds:Manufacturer>"
    "<tds:Model>MiBeeCam</tds:Model>"
    "<tds:FirmwareVersion>v0.1.0</tds:FirmwareVersion>"
    "<tds:SerialNumber>aabbccddeeff</tds:SerialNumber>"
    "<tds:HardwareId>ESP32-S3</tds:HardwareId>"
    "</tds:GetDeviceInformationResponse>"
    "</soap:Body>"
    "</soap:Envelope>";

static const char *G_CAPS_NOEV =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:tt=\"http://www.onvif.org/ver10/schema\""
    " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<soap:Body>"
    "<tds:GetCapabilitiesResponse>"
    "<tds:Capabilities>"
    "<tt:Device>"
    "<tt:XAddr>http://192.0.2.134:80/onvif/device_service</tt:XAddr>"
    "</tt:Device>"
    "<tt:Media>"
    "<tt:XAddr>http://192.0.2.134:80/onvif/media_service</tt:XAddr>"
    "</tt:Media>"
    "<tt:Analytics>"
    "<tt:XAddr>http://192.0.2.134:80/onvif/analytics_service</tt:XAddr>"
    "</tt:Analytics>"
    "</tds:Capabilities>"
    "</tds:GetCapabilitiesResponse>"
    "</soap:Body>"
    "</soap:Envelope>";

static const char *G_STREAM_URI =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:tt=\"http://www.onvif.org/ver10/schema\""
    " xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<soap:Body>"
    "<trt:GetStreamUriResponse>"
    "<trt:MediaUri>"
    "<tt:Uri>rtsp://192.0.2.134:554/stream</tt:Uri>"
    "<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
    "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
    "<tt:Timeout>PT10S</tt:Timeout>"
    "</trt:MediaUri>"
    "</trt:GetStreamUriResponse>"
    "</soap:Body>"
    "</soap:Envelope>";

static const char *G_FAULT =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:ter=\"http://www.onvif.org/ver10/error\">"
    "<soap:Body>"
    "<soap:Fault>"
    "<soap:Code>"
    "<soap:Value>soap:Sender</soap:Value>"
    "<soap:Subcode>"
    "<soap:Value>ter:ActionNotSupported</soap:Value>"
    "</soap:Subcode>"
    "</soap:Code>"
    "<soap:Reason>"
    "<soap:Text xml:lang=\"en\">Action not supported</soap:Text>"
    "</soap:Reason>"
    "</soap:Fault>"
    "</soap:Body>"
    "</soap:Envelope>";

static const char *G_PULL_EVENT =
    "<wsnt:NotificationMessage>"
    "<wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
    "tns1:VideoSource/MotionAlarm</wsnt:Topic>"
    "<wsnt:Message><tt:Message UtcTime=\"2026-09-20T07:42:13Z\">"
    "<tt:Source><tt:SimpleItem Name=\"Source\" Value=\"CSI\"/></tt:Source>"
    "<tt:Data>"
    "<tt:SimpleItem Name=\"State\" Value=\"true\"/>"
    "<tt:SimpleItem Name=\"Score\" Value=\"87\"/>"
    "</tt:Data></tt:Message></wsnt:Message>"
    "</wsnt:NotificationMessage>";

static const char *G_PROBE_MATCHES_HEAD =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<soap:Envelope"
    " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
    " xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
    " xmlns:wsdp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\">"
    "<soap:Header>"
    "<wsa:Action>"
    "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches"
    "</wsa:Action>"
    "<wsa:MessageID>urn:uuid:TESTUUID</wsa:MessageID>"
    "<wsa:RelatesTo>urn:uuid:probe-123</wsa:RelatesTo>"
    "<wsa:To>"
    "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"
    "</wsa:To>"
    "<wsd:AppSequence InstanceId=\"14200\" MessageNumber=\"1\"/>"
    "</soap:Header>"
    "<soap:Body>"
    "<wsd:ProbeMatches>"
    "<wsd:ProbeMatch>"
    "<wsa:EndpointReference>"
    "<wsa:Address>urn:uuid:TESTUUID</wsa:Address>"
    "</wsa:EndpointReference>"
    "<wsd:Types>tns:NetworkVideoTransmitter</wsd:Types>"
    "<wsd:Scopes>SCOPEBODY</wsd:Scopes>"
    "<wsd:XAddrs>http://192.0.2.134:80/onvif/device_service</wsd:XAddrs>"
    "<wsd:MetadataVersion>2</wsd:MetadataVersion>"
    "</wsd:ProbeMatch>"
    "</wsd:ProbeMatches>"
    "</soap:Body>"
    "</soap:Envelope>";

/* ---- tests ---- */

static void test_device_service_xml(void)
{
    struct tm utc = { .tm_hour = 7, .tm_min = 42, .tm_sec = 13,
                      .tm_year = 126, .tm_mon = 8, .tm_mday = 20 };
    onvif_xml_system_date_and_time(g, sizeof(g), &utc);
    CHECK_STR(g, G_SYSTEM_DATE, "GetSystemDateAndTime golden");

    onvif_xml_device_information(g, sizeof(g), "MiBee", "MiBeeCam",
                                 "v0.1.0", "aabbccddeeff", "ESP32-S3");
    CHECK_STR(g, G_DEVICE_INFO, "GetDeviceInformation golden");

    int n = onvif_xml_capabilities(g, sizeof(g), "192.0.2.134", false);
    CHECK(n > 0 && (size_t)n < sizeof(g), "capabilities fits");
    CHECK_STR(g, G_CAPS_NOEV, "GetCapabilities (no events) golden");

    n = onvif_xml_capabilities(g, sizeof(g), "192.0.2.134", true);
    CHECK(strstr(g, "/onvif/events_service") != NULL,
          "capabilities advertises events when enabled");
    CHECK(strstr(g, "<tt:Events>") != NULL, "events element present");

    n = onvif_xml_profiles(g, sizeof(g), 12);
    CHECK(n > 0 && strstr(g, "<tt:FrameRateLimit>12</tt:FrameRateLimit>"),
          "GetProfiles embeds frame rate");

    onvif_xml_stream_uri(g, sizeof(g), "rtsp://192.0.2.134:554/stream");
    CHECK_STR(g, G_STREAM_URI, "GetStreamUri golden");

    n = onvif_xml_snapshot_uri(g, sizeof(g), "http://192.0.2.134:80/api/capture");
    CHECK(strstr(g, "<tt:Uri>http://192.0.2.134:80/api/capture</tt:Uri>") != NULL,
          "GetSnapshotUri embeds URI");

    onvif_xml_fault_action_not_supported(g, sizeof(g));
    CHECK_STR(g, G_FAULT, "Sender/ActionNotSupported fault golden");
}

static void test_events_xml(void)
{
    int n = onvif_xml_create_pull_point_response(
        g, sizeof(g), "192.0.2.134", "2026-09-20T07:42:13Z",
        "2026-09-20T08:42:13Z");
    CHECK(n > 0 && strstr(g, "CreatePullPointSubscriptionResponse") &&
          strstr(g, "http://192.0.2.134:80/onvif/events_service") &&
          strstr(g, "<wsnt:TerminationTime>2026-09-20T08:42:13Z"),
          "CreatePullPointSubscription shape");

    n = onvif_xml_renew_response(g, sizeof(g), "2026-09-20T09:00:00Z");
    CHECK(strstr(g, "<wsnt:TerminationTime>2026-09-20T09:00:00Z"),
          "RenewResponse termination");

    CHECK(strstr(onvif_xml_unsubscribe_response(), "UnsubscribeResponse"),
          "UnsubscribeResponse static");

    onvif_xml_pull_event(g, sizeof(g), "2026-09-20T07:42:13Z", true, 87);
    CHECK_STR(g, G_PULL_EVENT, "MotionAlarm NotificationMessage golden");

    n = onvif_xml_pull_event(g, sizeof(g), "2026-09-20T07:42:14Z", false, 3);
    CHECK(strstr(g, "Value=\"false\"") && strstr(g, "Value=\"3\""),
          "cleared event fields");

    n = onvif_xml_events_fault(g, sizeof(g), "ter:SubscriptionReferenceDereferenced",
                               "no active subscription (expired)");
    CHECK(strstr(g, "ter:SubscriptionReferenceDereferenced") &&
          strstr(g, "no active subscription"),
          "events fault");

    /* truncation contract: builders report would-be length, never write past n */
    char small[32];
    n = onvif_xml_stream_uri(small, sizeof(small), "rtsp://x:554/stream");
    CHECK(n > 0 && (size_t)n >= sizeof(small), "truncation reported");
}

static void test_probe(void)
{
    CHECK(onvif_probe_is_probe("<soap:Body><wsdiscovery:Probe/></soap:Body>"),
          "wsdiscovery:Probe detected");
    CHECK(onvif_probe_is_probe("<a:Probe xmlns:a=\"x\"/>"), "bare :Probe");
    CHECK(!onvif_probe_is_probe("<wsd:ProbeMatches>x</wsd:ProbeMatches>"),
          "ProbeMatches echo ignored");
    CHECK(!onvif_probe_is_probe("<svc:Hello/>"), "Hello ignored");

    char mid[64];
    CHECK(onvif_probe_message_id(
              "<wsa:MessageID>urn:uuid:probe-123</wsa:MessageID>", mid,
              sizeof(mid)) &&
          strcmp(mid, "urn:uuid:probe-123") == 0,
          "MessageID extracted");
    CHECK(!onvif_probe_message_id("<no-id/>", mid, sizeof(mid)),
          "missing MessageID rejected");

    int n = onvif_probe_build_matches(g, sizeof(g), "urn:uuid:probe-123",
                                      "TESTUUID", "192.0.2.134", "SCOPEBODY");
    CHECK(n > 0 && (size_t)n < sizeof(g), "ProbeMatches fits");
    CHECK_STR(g, G_PROBE_MATCHES_HEAD, "ProbeMatches golden");

    n = onvif_probe_build_matches(g, sizeof(g), NULL, "TESTUUID",
                                  "192.0.2.134", "SCOPEBODY");
    CHECK(strstr(g, "<wsa:RelatesTo></wsa:RelatesTo>") != NULL,
          "missing RelatesTo becomes empty element");

    n = onvif_probe_build_hello(g, sizeof(g), "TESTUUID", "192.0.2.134",
                                "SCOPEBODY");
    CHECK(n > 0 && strstr(g, "<wsd:Hello>") &&
          strstr(g, "discovery/Hello</wsa:Action>") &&
          strstr(g, "urn:uuid:TESTUUID"),
          "Hello shape");
}

static void test_ring(void)
{
    onvif_c_event_ring_t r;
    onvif_c_ring_reset(&r);

    onvif_c_event_t e;
    CHECK(!onvif_c_ring_pop(&r, &e), "pop on empty");

    onvif_c_ring_push(&r, true, 50, 1000);
    onvif_c_ring_push(&r, false, 10, 2000);
    CHECK(r.count == 2 && r.generated == 2, "two pushed");

    CHECK(onvif_c_ring_pop(&r, &e) && e.active && e.score == 50 && e.utc == 1000,
          "FIFO order");
    CHECK(onvif_c_ring_pop(&r, &e) && !e.active, "second event");
    CHECK(!onvif_c_ring_pop(&r, &e), "drained");

    /* overflow drops the oldest */
    onvif_c_ring_reset(&r);
    for (int i = 0; i < ONVIF_C_EVENT_QUEUE_MAX + 3; i++) {
        onvif_c_ring_push(&r, true, (uint8_t)i, 3000 + i);
    }
    CHECK(r.count == ONVIF_C_EVENT_QUEUE_MAX, "ring capped");
    onvif_c_ring_pop(&r, &e);
    CHECK(e.score == 3, "oldest dropped on overflow (first survivor = #3)");
    CHECK(r.generated == ONVIF_C_EVENT_QUEUE_MAX + 3, "generated counts all");

    onvif_c_ring_reset(&r);
    CHECK(r.count == 0 && r.generated == 0 && r.head == 0, "reset clears");
}

int main(void)
{
    test_device_service_xml();
    test_events_xml();
    test_probe();
    test_ring();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
