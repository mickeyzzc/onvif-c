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
static int checks   = 0;

#define CHECK(cond, name)                                                                          \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(cond)) {                                                                             \
            failures++;                                                                            \
            printf("FAIL: %s\n", name);                                                            \
        }                                                                                          \
    } while (0)

#define CHECK_STR(buf, expected, name)                                                             \
    do {                                                                                           \
        checks++;                                                                                  \
        if (strcmp(buf, expected) != 0) {                                                          \
            failures++;                                                                            \
            printf("FAIL: %s\n  got:  %s\n  want: %s\n", name, buf, expected);                     \
        }                                                                                          \
    } while (0)

static char g[4096];

/* ---- golden strings (extracted from field-proven firmware output) ---- */

static const char *G_SYSTEM_DATE = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
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

static const char *G_DEVICE_INFO = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
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

static const char *G_STREAM_URI = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
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

static const char *G_FAULT = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
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
    struct tm utc = {
        .tm_hour = 7, .tm_min = 42, .tm_sec = 13, .tm_year = 126, .tm_mon = 8, .tm_mday = 20};
    onvif_xml_system_date_and_time(g, sizeof(g), &utc);
    CHECK_STR(g, G_SYSTEM_DATE, "GetSystemDateAndTime golden");

    onvif_xml_device_information(g, sizeof(g), "MiBee", "MiBeeCam", "v0.1.0", "aabbccddeeff",
                                 "ESP32-S3");
    CHECK_STR(g, G_DEVICE_INFO, "GetDeviceInformation golden");

    int n = onvif_xml_capabilities(g, sizeof(g), "192.0.2.134", 80, false);
    CHECK(n > 0 && (size_t)n < sizeof(g), "capabilities fits");
    CHECK_STR(g, G_CAPS_NOEV, "GetCapabilities (no events) golden");

    n = onvif_xml_capabilities(g, sizeof(g), "192.0.2.134", 80, true);
    CHECK(strstr(g, "/onvif/events_service") != NULL,
          "capabilities advertises events when enabled");
    CHECK(strstr(g, "<tt:Events>") != NULL, "events element present");

    /* http_port is injected, never hardcoded: XAddrs must follow it */
    n = onvif_xml_capabilities(g, sizeof(g), "192.0.2.134", 8080, true);
    CHECK(n > 0 && strstr(g, "http://192.0.2.134:8080/onvif/device_service") &&
              strstr(g, "http://192.0.2.134:8080/onvif/media_service") &&
              strstr(g, "http://192.0.2.134:8080/onvif/events_service") &&
              strstr(g, "http://192.0.2.134:8080/onvif/analytics_service"),
          "GetCapabilities honors non-default http port");

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
    int n = onvif_xml_create_pull_point_response(g, sizeof(g), "192.0.2.134", 80,
                                                 "2026-09-20T07:42:13Z", "2026-09-20T08:42:13Z");
    CHECK(n > 0 && strstr(g, "CreatePullPointSubscriptionResponse") &&
              strstr(g, "http://192.0.2.134:80/onvif/events_service") &&
              strstr(g, "<wsnt:TerminationTime>2026-09-20T08:42:13Z"),
          "CreatePullPointSubscription shape");

    n = onvif_xml_create_pull_point_response(g, sizeof(g), "192.0.2.134", 8080,
                                             "2026-09-20T07:42:13Z", "2026-09-20T08:42:13Z");
    CHECK(strstr(g, "http://192.0.2.134:8080/onvif/events_service") != NULL,
          "CreatePullPointSubscription honors non-default http port");

    n = onvif_xml_renew_response(g, sizeof(g), "2026-09-20T09:00:00Z");
    CHECK(strstr(g, "<wsnt:TerminationTime>2026-09-20T09:00:00Z"), "RenewResponse termination");

    CHECK(strstr(onvif_xml_unsubscribe_response(), "UnsubscribeResponse"),
          "UnsubscribeResponse static");

    onvif_xml_pull_event(g, sizeof(g), "2026-09-20T07:42:13Z", true, 87);
    CHECK_STR(g, G_PULL_EVENT, "MotionAlarm NotificationMessage golden");

    n = onvif_xml_pull_event(g, sizeof(g), "2026-09-20T07:42:14Z", false, 3);
    CHECK(strstr(g, "Value=\"false\"") && strstr(g, "Value=\"3\""), "cleared event fields");

    n = onvif_xml_events_fault(g, sizeof(g), "ter:SubscriptionReferenceDereferenced",
                               "no active subscription (expired)");
    CHECK(strstr(g, "ter:SubscriptionReferenceDereferenced") && strstr(g, "no active subscription"),
          "events fault");

    /* truncation contract: builders report would-be length, never write past n */
    char small[32];
    n = onvif_xml_stream_uri(small, sizeof(small), "rtsp://x:554/stream");
    CHECK(n > 0 && (size_t)n >= sizeof(small), "truncation reported");

    /* every prefix length of the multi-stage capabilities builder: all
     * early-return truncation paths report a length and never overrun */
    {
        bool clean = true;
        char buf[1024];
        for (size_t len = 1; len <= 900; len++) {
            int r = onvif_xml_capabilities(buf, len, "192.0.2.134", 80, true);
            if (r < 0) {
                clean = false;
            }
        }
        CHECK(clean, "capabilities truncation sweep clean");
    }
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
    CHECK(onvif_probe_message_id("<wsa:MessageID>urn:uuid:probe-123</wsa:MessageID>", mid,
                                 sizeof(mid)) &&
              strcmp(mid, "urn:uuid:probe-123") == 0,
          "MessageID extracted");
    CHECK(!onvif_probe_message_id("<no-id/>", mid, sizeof(mid)), "missing MessageID rejected");
    CHECK(!onvif_probe_message_id("<wsa:MessageID>", mid, sizeof(mid)),
          "MessageID without close tag rejected");
    CHECK(!onvif_probe_message_id("<wsa:MessageID></wsa:MessageID>", mid, sizeof(mid)),
          "empty MessageID rejected");
    CHECK(!onvif_probe_message_id("<wsa:MessageID>0123456789012345678901234567890123456789"
                                  "012345678901234567890123</wsa:MessageID>",
                                  mid, 32),
          "oversized MessageID rejected");

    int n = onvif_probe_build_matches(g, sizeof(g), "urn:uuid:probe-123", "TESTUUID", "192.0.2.134",
                                      80, "SCOPEBODY");
    CHECK(n > 0 && (size_t)n < sizeof(g), "ProbeMatches fits");
    CHECK_STR(g, G_PROBE_MATCHES_HEAD, "ProbeMatches golden");

    n = onvif_probe_build_matches(g, sizeof(g), NULL, "TESTUUID", "192.0.2.134", 80, "SCOPEBODY");
    CHECK(strstr(g, "<wsa:RelatesTo></wsa:RelatesTo>") != NULL,
          "missing RelatesTo becomes empty element");

    n = onvif_probe_build_matches(g, sizeof(g), "urn:uuid:probe-123", "TESTUUID", "192.0.2.134",
                                  8080, "SCOPEBODY");
    CHECK(strstr(g, "http://192.0.2.134:8080/onvif/device_service") != NULL,
          "ProbeMatches honors non-default http port");

    n = onvif_probe_build_hello(g, sizeof(g), "TESTUUID", "192.0.2.134", 80, "SCOPEBODY");
    CHECK(n > 0 && strstr(g, "<wsd:Hello>") && strstr(g, "discovery/Hello</wsa:Action>") &&
              strstr(g, "urn:uuid:TESTUUID"),
          "Hello shape");

    n = onvif_probe_build_hello(g, sizeof(g), "TESTUUID", "192.0.2.134", 8080, "SCOPEBODY");
    CHECK(strstr(g, "http://192.0.2.134:8080/onvif/device_service") != NULL,
          "Hello honors non-default http port");
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

    CHECK(onvif_c_ring_pop(&r, &e) && e.active && e.score == 50 && e.utc == 1000, "FIFO order");
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

/* Completion batch goldens (issues #13/#14/#15/#16). */
static const char *G_NEW_SERVICES =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
    "xmlns:tds=\"http://www.onvif.org/v"
    "er10/device/wsdl\"><soap:Body><tds:GetServicesResponse><tds:Service><tds:Namespace>http://www"
    ".onvif.org/ver10/device/wsdl</tds:Namespace><tds:XAddr>http://192.0.2.134:8080/onvif/device_"
    "service</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></"
    "tds:Service><tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace"
    "><tds:XAddr>http://192.0.2.134:8080/onvif/media_service</tds:XAddr><tds:Version><tt:Major>2<"
    "/tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service><tds:Service><tds:Namespace>http"
    "://www.onvif.org/ver20/analytics/wsdl</tds:Namespace><tds:XAddr>http://192.0.2.134:8080/onvi"
    "f/analytics_service</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></td"
    "s:Version></tds:Service></tds:GetServicesResponse></soap:Body></soap:Envelope>";
static const char *G_NEW_REBOOT =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"><soap:Body><tds:SystemReboo"
    "tResponse><tds:Message>Device rebooting</tds:Message></tds:SystemRebootResponse></soap:Body>"
    "</soap:Envelope>";
static const char *G_NEW_SETDATE =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"><soap:Body><tds:SetSystemDa"
    "teAndTimeResponse/></soap:Body></soap:Envelope>";
static const char *G_NEW_DEVCAPS =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"><soap:Body><tds:GetServiceC"
    "apabilitiesResponse><tds:Capabilities Network=\"false\" "
    "System=\"false\"/></tds:GetServiceCapabi"
    "litiesResponse></soap:Body></soap:Envelope>";
static const char *G_NEW_VIDEOSOURCES =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
    "xmlns:trt=\"http://www.onvif.org/v"
    "er10/media/wsdl\"><soap:Body><trt:GetVideoSourcesResponse><trt:VideoSources token=\"VideoSourc"
    "e_1\"><tt:Framerate>12</tt:Framerate><tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</t"
    "t:Height></tt:Resolution></trt:VideoSources></trt:GetVideoSourcesResponse></soap:Body></soap"
    ":Envelope>";
static const char *G_NEW_SETENC =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"><soap:Body><trt:SetVideoEnco"
    "derConfigurationResponse/></soap:Body></soap:Envelope>";
static const char *G_NEW_GUARANTEED =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"><soap:Body><trt:GetGuarantee"
    "dNumberOfVideoEncoderInstancesResponse><trt:TotalInstances>1</trt:TotalInstances></trt:GetGu"
    "aranteedNumberOfVideoEncoderInstancesResponse></soap:Body></soap:Envelope>";
static const char *G_NEW_SYNC =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"><soap:Body><trt:SetSynchroni"
    "zationPointResponse/></soap:Body></soap:Envelope>";
static const char *G_NEW_MEDIACAPS =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"><soap:Body><trt:GetServiceCa"
    "pabilitiesResponse><trt:Capabilities SnapshotUri=\"true\" RTP_US=\"false\" "
    "RTP_Multicast=\"false\""
    " RTP_TCP=\"true\" "
    "NonFixedIP=\"false\"/></trt:GetServiceCapabilitiesResponse></soap:Body></soap:"
    "Envelope>";
static const char *G_NEW_EVPROPS =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope "
    "xmlns:s=\"http://www.w3.org/2003/05/soap-en"
    "velope\" xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
    "xmlns:wsnt=\"http://docs.oasis-ope"
    "n.org/wsn/b-2\"><s:Body><tev:GetEventPropertiesResponse><tev:TopicNamespaceLocation>http://ww"
    "w.onvif.org/ver10/topics</tev:TopicNamespaceLocation><wsnt:FixedTopicSet>true</wsnt:FixedTop"
    "icSet><tev:TopicExpressionDialect>http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet"
    "</tev:TopicExpressionDialect><tev:MessageContentFilterDialect>http://www.onvif.org/ver10/tev"
    "/messageContentFilter/ItemFilter</tev:MessageContentFilterDialect></tev:GetEventPropertiesRe"
    "sponse></s:Body></s:Envelope>";
static const char *G_NEW_EVCAPS =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope "
    "xmlns:s=\"http://www.w3.org/2003/05/soap-en"
    "velope\" "
    "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\"><s:Body><tev:GetServiceCapabiliti"
    "esResponse><tev:Capabilities WSSubscriptionPolicySupport=\"false\" "
    "WSPullPointSupport=\"true\" W"
    "SPausableSubscriptionManagerInterfaceSupport=\"false\"/></tev:GetServiceCapabilitiesResponse><"
    "/s:Body></s:Envelope>";
static const char *G_NEW_EVSYNC =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope "
    "xmlns:s=\"http://www.w3.org/2003/05/soap-en"
    "velope\" "
    "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\"><s:Body><tev:SetSynchronizationPo"
    "intResponse/></s:Body></s:Envelope>";
static const char *G_NEW_BYE =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
    "xmlns:wsd=\"http:/"
    "/schemas.xmlsoap.org/ws/2005/04/discovery\"><soap:Header><wsa:MessageID>urn:uuid:11111111-222"
    "2-3333-4444-555555555555</wsa:MessageID><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery"
    "</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Bye</wsa:Action><wsd:Ap"
    "pSequence InstanceId=\"1\" "
    "MessageNumber=\"2\"/></soap:Header><soap:Body><wsd:Bye><wsa:EndpointR"
    "eference><wsa:Address>urn:uuid:11111111-2222-3333-4444-555555555555</wsa:Address></wsa:Endpo"
    "intReference></wsd:Bye></soap:Body></soap:Envelope>";
static const char *G_NEW_RESOLVEM =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
    "xmlns:wsd=\"http:/"
    "/schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:wsdp=\"http://schemas.xmlsoap.org/ws/2006/02"
    "/devprof\"><soap:Header><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ResolveMa"
    "tches</wsa:Action><wsa:MessageID>urn:uuid:11111111-2222-3333-4444-555555555555</wsa:MessageI"
    "D><wsa:RelatesTo>urn:uuid:rel-1</wsa:RelatesTo><wsa:To>http://schemas.xmlsoap.org/ws/2004/08"
    "/addressing/role/anonymous</wsa:To><wsd:AppSequence InstanceId=\"14200\" "
    "MessageNumber=\"1\"/></"
    "soap:Header><soap:Body><wsd:ResolveMatches><wsd:ResolveMatch><wsa:EndpointReference><wsa:Add"
    "ress>urn:uuid:11111111-2222-3333-4444-555555555555</wsa:Address></wsa:EndpointReference><wsd"
    ":XAddrs>http://192.0.2.134:8080/onvif/device_service</wsd:XAddrs><wsd:MetadataVersion>2</wsd"
    ":MetadataVersion></wsd:ResolveMatch></wsd:ResolveMatches></soap:Body></soap:Envelope>";
static const char *G_NEW_SCOPES =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope "
    "xmlns:soap=\"http://www.w3.org/2003/05/s"
    "oap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
    "xmlns:tds=\"http://www.onvif.org/v"
    "er10/device/wsdl\"><soap:Body><tds:GetScopesResponse><tt:Scopes><tt:ScopeDef>Fixed</tt:ScopeD"
    "ef><tt:ScopeItem>onvif://www.onvif.org/type/video_encoder</tt:ScopeItem></tt:Scopes><tt:Scop"
    "es><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/name/MiBeeCam</tt:Sco"
    "peItem></tt:Scopes></tds:GetScopesResponse></soap:Body></soap:Envelope>";

int main(void)
{
    test_device_service_xml();
    test_events_xml();
    test_probe();
    test_ring();

    /* ---- completion batch checks ---- */
    int n = onvif_xml_services(g, sizeof g, "192.0.2.134", 8080, false);
    CHECK(n > 0 && (size_t)n < sizeof g, "services_noev built");
    CHECK_STR(g, G_NEW_SERVICES, "services_noev golden");
    n = onvif_xml_system_reboot(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "reboot built");
    CHECK_STR(g, G_NEW_REBOOT, "reboot golden");
    n = onvif_xml_set_system_date_and_time_ack(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "setdate built");
    CHECK_STR(g, G_NEW_SETDATE, "setdate golden");
    n = onvif_xml_device_service_capabilities(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "devcaps built");
    CHECK_STR(g, G_NEW_DEVCAPS, "devcaps golden");
    n = onvif_xml_video_sources(g, sizeof g, "VideoSource_1", 640, 480, 12);
    CHECK(n > 0 && (size_t)n < sizeof g, "videosources built");
    CHECK_STR(g, G_NEW_VIDEOSOURCES, "videosources golden");
    n = onvif_xml_set_video_encoder_configuration_ack(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "setenc built");
    CHECK_STR(g, G_NEW_SETENC, "setenc golden");
    n = onvif_xml_guaranteed_encoder_instances(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "guaranteed built");
    CHECK_STR(g, G_NEW_GUARANTEED, "guaranteed golden");
    n = onvif_xml_set_synchronization_point_ack(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "sync built");
    CHECK_STR(g, G_NEW_SYNC, "sync golden");
    n = onvif_xml_media_service_capabilities(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "mediacaps built");
    CHECK_STR(g, G_NEW_MEDIACAPS, "mediacaps golden");
    n = onvif_xml_event_properties(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "evprops built");
    CHECK_STR(g, G_NEW_EVPROPS, "evprops golden");
    n = onvif_xml_events_service_capabilities(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "evcaps built");
    CHECK_STR(g, G_NEW_EVCAPS, "evcaps golden");
    n = onvif_xml_events_sync_point_ack(g, sizeof g);
    CHECK(n > 0 && (size_t)n < sizeof g, "evsync built");
    CHECK_STR(g, G_NEW_EVSYNC, "evsync golden");
    n = onvif_probe_build_bye(g, sizeof g, "11111111-2222-3333-4444-555555555555");
    CHECK(n > 0 && (size_t)n < sizeof g, "bye built");
    CHECK_STR(g, G_NEW_BYE, "bye golden");
    n = onvif_probe_build_resolve_matches(
        g, sizeof g, "urn:uuid:rel-1", "11111111-2222-3333-4444-555555555555", "192.0.2.134", 8080);
    CHECK(n > 0 && (size_t)n < sizeof g, "resolvem built");
    CHECK_STR(g, G_NEW_RESOLVEM, "resolvem golden");
    n = onvif_xml_get_scopes(
        g, sizeof g,
        "onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/name/MiBeeCam");
    CHECK(n > 0 && (size_t)n < sizeof g, "scopes built");
    CHECK_STR(g, G_NEW_SCOPES, "scopes golden");
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
