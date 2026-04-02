#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "handlers.h"
#include "web_context.h"
#include <Arduino.h>

ESP8266WebServer server(8080);
WebSocketsClient wsClient;

static unsigned long lastDDNS = 0;

// ── DuckDNS ──────────────────────────────────────────────────────────────────
void updateDuckDNS() {
  WiFiClient client;
  HTTPClient http;
  String url = "http://www.duckdns.org/update?domains=" DUCKDNS_DOMAIN
               "&token=" DUCKDNS_TOKEN "&ip=";
  if (http.begin(client, url)) {
    int code = http.GET();
    Serial.print("DuckDNS: ");
    Serial.println(code > 0 ? http.getString() : "failed");
    http.end();
  }
}

// ── Request router (shared by local HTTP + WebSocket tunnel) ─────────────────
void routeRequest(WebContext& ctx, const String& method, const String& path) {
  if (path == "/data") {
    if      (method == "POST")    {
      if (ctx.hasArg("plain") && ctx.arg("plain").startsWith("[")) handleBulkCreate(ctx);
      else handleCreate(ctx);
    }
    else if (method == "GET")     handleRead(ctx);
    else if (method == "DELETE")  handleDelete(ctx);
    else if (method == "PUT")     handleUpdate(ctx);
    else if (method == "OPTIONS") { sendAuthCORS(ctx); ctx.send(200, "text/plain", ""); }
    else                          ctx.send(405, "text/plain", "Method not allowed");
  } else if (path == "/space" && method == "GET") {
    handleSpace(ctx);
  } else {
    ctx.send(404, "text/plain", "Not found");
  }
}

// ── WebSocket relay event handler ─────────────────────────────────────────────
void onWsEvent(WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Relay: connected");
      break;
    case WStype_DISCONNECTED:
      Serial.println("Relay: disconnected");
      break;
    case WStype_TEXT: {
      JsonDocument req;
      if (deserializeJson(req, payload, len)) return;

      String id     = req["id"]     | "";
      String method = req["method"] | "GET";
      String path   = req["path"]   | "/";

      TunnelContext ctx;

      if (req["query"].is<JsonObject>())
        for (JsonPair kv : req["query"].as<JsonObject>())
          ctx.setArg(kv.key().c_str(), kv.value().as<String>());

      String body = req["body"] | "";
      if (body.length()) ctx.setArg("plain", body);

      if (req["headers"].is<JsonObject>())
        for (JsonPair kv : req["headers"].as<JsonObject>())
          ctx.setHeader(kv.key().c_str(), kv.value().as<String>());

      routeRequest(ctx, method, path);
      String resp = ctx.toJson(id);
      wsClient.sendTXT(resp);
      break;
    }
    default: break;
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  IPAddress ip, gw, sn, dns;
  ip.fromString(STATIC_IP);
  gw.fromString(GATEWAY_IP);
  sn.fromString(SUBNET_MASK);
  dns.fromString(DNS_SERVER);
  WiFi.config(ip, gw, sn, dns);

  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // ── Local HTTP routes ──────────────────────────────────────────────────────
  server.on("/data", HTTP_POST, []() {
    ServerContext ctx(server);
    if (server.hasArg("plain") && server.arg("plain").startsWith("[")) handleBulkCreate(ctx);
    else handleCreate(ctx);
  });
  server.on("/data",  HTTP_GET,    []() { ServerContext ctx(server); handleRead(ctx);   });
  server.on("/data",  HTTP_DELETE, []() { ServerContext ctx(server); handleDelete(ctx); });
  server.on("/data",  HTTP_PUT,    []() { ServerContext ctx(server); handleUpdate(ctx); });
  server.on("/space", HTTP_GET,    []() { ServerContext ctx(server); handleSpace(ctx);  });

  server.onNotFound([&]() {
    if (server.method() == HTTP_OPTIONS) {
      ServerContext ctx(server);
      sendAuthCORS(ctx);
      server.send(200);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
  Serial.println("Local server on :8080");

  // ── DuckDNS ────────────────────────────────────────────────────────────────
  updateDuckDNS();
  lastDDNS = millis();

  // ── WebSocket relay ────────────────────────────────────────────────────────
  wsClient.beginSSL(RELAY_HOST, RELAY_PORT, RELAY_PATH);
  wsClient.onEvent(onWsEvent);
  wsClient.setReconnectInterval(5000);
  Serial.println("Connecting to relay...");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.begin(SSID, PASS);
    delay(1000);
    return;
  }

  server.handleClient();
  wsClient.loop();

  if (millis() - lastDDNS > 300000UL) {
    updateDuckDNS();
    lastDDNS = millis();
  }
}
