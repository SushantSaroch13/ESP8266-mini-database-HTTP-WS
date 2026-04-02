#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// Abstract request/response interface — implemented by both local HTTP and WebSocket tunnel
class WebContext {
public:
  virtual bool   hasArg(const String& name)    const = 0;
  virtual String arg(const String& name)        const = 0;
  virtual bool   hasHeader(const String& name)  const = 0;
  virtual String header(const String& name)     const = 0;
  virtual void   send(int code, const char* type, const String& body) = 0;
  virtual void   sendHeader(const String& name, const String& value)  = 0;
  virtual ~WebContext() {}
};

// ── Local HTTP (wraps ESP8266WebServer) ──────────────────────────────────────
class ServerContext : public WebContext {
  ESP8266WebServer& _srv;
public:
  explicit ServerContext(ESP8266WebServer& srv) : _srv(srv) {}
  bool   hasArg(const String& n)   const override { return _srv.hasArg(n); }
  String arg(const String& n)       const override { return _srv.arg(n); }
  bool   hasHeader(const String& n) const override { return _srv.hasHeader(n); }
  String header(const String& n)    const override { return _srv.header(n); }
  void   send(int c, const char* t, const String& b) override { _srv.send(c, t, b); }
  void   sendHeader(const String& n, const String& v) override { _srv.sendHeader(n.c_str(), v.c_str()); }
};

// ── WebSocket tunnel ─────────────────────────────────────────────────────────
class TunnelContext : public WebContext {
  static const int MAX_KV = 12;
  struct KV { String k, v; };

  KV  _args[MAX_KV];  int _argN  = 0;
  KV  _hdrs[MAX_KV];  int _hdrN  = 0;
  KV  _rhdrs[8];      int _rhdrN = 0;
  int    _status = 200;
  String _ctype, _body;

public:
  void setArg(const String& k, const String& v)    { if (_argN  < MAX_KV) _args[_argN++]   = {k, v}; }
  void setHeader(const String& k, const String& v) { if (_hdrN  < MAX_KV) _hdrs[_hdrN++]   = {k, v}; }

  bool hasArg(const String& n) const override {
    for (int i = 0; i < _argN; i++)
      if (_args[i].k == n) return true;
    return false;
  }
  String arg(const String& n) const override {
    for (int i = 0; i < _argN; i++)
      if (_args[i].k == n) return _args[i].v;
    return "";
  }
  bool hasHeader(const String& n) const override {
    for (int i = 0; i < _hdrN; i++)
      if (_hdrs[i].k.equalsIgnoreCase(n)) return true;
    return false;
  }
  String header(const String& n) const override {
    for (int i = 0; i < _hdrN; i++)
      if (_hdrs[i].k.equalsIgnoreCase(n)) return _hdrs[i].v;
    return "";
  }
  void send(int code, const char* type, const String& body) override {
    _status = code; _ctype = type; _body = body;
  }
  void sendHeader(const String& name, const String& value) override {
    if (_rhdrN < 8) _rhdrs[_rhdrN++] = {name, value};
  }

  // Serialise response back to the relay
  String toJson(const String& id) const {
    JsonDocument doc;
    doc["id"]          = id;
    doc["status"]      = _status;
    doc["contentType"] = _ctype;
    doc["body"]        = _body;
    JsonObject h = doc["headers"].to<JsonObject>();
    for (int i = 0; i < _rhdrN; i++) h[_rhdrs[i].k] = _rhdrs[i].v;
    String out;
    serializeJson(doc, out);
    return out;
  }
};
