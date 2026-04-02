#pragma once
#include "web_context.h"
#include <base64.h>
#include "config.h"

// ---------- INLINE CORS ----------
void sendAuthCORS(WebContext& ctx) {
  ctx.sendHeader("Access-Control-Allow-Origin", "*");
  ctx.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  ctx.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

// ---------- BASE64 CHECK ----------
bool checkAuthHeader(String authHeader) {
  if (!authHeader.startsWith("Basic ")) return false;
  String encoded  = authHeader.substring(6);
  String expected = base64::encode(String(BASIC_USER) + ":" + String(BASIC_PASS));
  return encoded == expected;
}

// ---------- AUTH ----------
bool isAuthorized(WebContext& ctx) {
  if (!ctx.hasHeader("Authorization")) {
    sendAuthCORS(ctx);
    ctx.send(401, "text/plain", "Missing Authorization header");
    return false;
  }
  if (!checkAuthHeader(ctx.header("Authorization"))) {
    sendAuthCORS(ctx);
    ctx.send(401, "text/plain", "Invalid credentials");
    return false;
  }
  return true;
}
