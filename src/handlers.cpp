#include "handlers.h"
#include "binary_utils.h"
#include "config.h"
#include "auth.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// ---------- CORS SAFE SEND ----------
static void sendWithCORS(WebContext &ctx, int code, const char* type, const String& body) {
  ctx.sendHeader("Access-Control-Allow-Origin", "*");
  ctx.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  ctx.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  ctx.send(code, type, body);
}

// ---------- ID ----------
int getNextId() {
  int id = 1;

  File file = LittleFS.open(META_PATH, "r");
  if (file) {
    id = file.readString().toInt();
    file.close();
  }

  File wfile = LittleFS.open(META_PATH, "w");
  wfile.print(id + 1);
  wfile.close();

  return id;
}

// ---------- SPACE ----------
void handleSpace(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  FSInfo fs_info;
  LittleFS.info(fs_info);

  JsonDocument doc;

  doc["totalBytes"]    = fs_info.totalBytes;
  doc["usedBytes"]     = fs_info.usedBytes;
  doc["freeBytes"]     = fs_info.totalBytes - fs_info.usedBytes;
  doc["approxEntries"] = (fs_info.totalBytes - fs_info.usedBytes) / sizeof(Record);

  String out;
  serializeJson(doc, out);

  sendWithCORS(ctx, 200, "application/json", out);
}

// ---------- CLEAN ----------
void cleanupIfNeeded() {
  FSInfo fs_info;
  LittleFS.info(fs_info);

  size_t freeBytes = fs_info.totalBytes - fs_info.usedBytes;
  if (freeBytes > 5000) return;

  File file = LittleFS.open(FILE_PATH, "r");
  if (!file) return;

  File temp = LittleFS.open("/temp.bin", "w");
  if (!temp) {
    file.close();
    return;
  }

  int skipCount = 20, current = 0;
  while (file.available()) {
    Record rec;
    file.read((uint8_t*)&rec, sizeof(Record));
    if (current++ < skipCount) continue;
    temp.write((uint8_t*)&rec, sizeof(Record));
  }

  file.close();
  temp.close();

  LittleFS.remove(FILE_PATH);
  LittleFS.rename("/temp.bin", FILE_PATH);
}

// ---------- CREATE ----------
void handleCreate(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  if (!ctx.hasArg("plain"))
    return sendWithCORS(ctx, 400, "text/plain", "No body");

  JsonDocument doc;
  if (deserializeJson(doc, ctx.arg("plain")))
    return sendWithCORS(ctx, 400, "text/plain", "Invalid JSON");

  int id = getNextId();
  Record rec;
  jsonToRecord(doc.as<JsonObject>(), rec, id);

  File file = LittleFS.open(FILE_PATH, "a");
  file.write((uint8_t*)&rec, sizeof(Record));
  file.close();

  cleanupIfNeeded();

  sendWithCORS(ctx, 200, "text/plain", "Created");
}

// ---------- READ ----------
void handleRead(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  String idParam = ctx.arg("id");
  int cursor = ctx.arg("cursor").toInt();
  int limit = ctx.arg("limit").toInt();
  if (limit <= 0) limit = 10;

  int rangeStart = -1, rangeEnd = -1;
  if (idParam.indexOf('-') != -1) {
    int dash = idParam.indexOf('-');
    rangeStart = idParam.substring(0, dash).toInt();
    rangeEnd   = idParam.substring(dash + 1).toInt();
  } else if (idParam.length() > 0) {
    rangeStart = rangeEnd = idParam.toInt();
  }

  File file = LittleFS.open(FILE_PATH, "r");
  if (!file)
    return sendWithCORS(ctx, 200, "application/json", "[]");

  size_t totalRecords = file.size() / sizeof(Record);
  int count = 0;
  String response = "[";
  bool first = true;

  for (int i = totalRecords - 1; i >= 0; i--) {
    file.seek(i * sizeof(Record), SeekSet);
    Record rec;
    file.read((uint8_t*)&rec, sizeof(Record));
    int id = rec.id;

    if (cursor > 0 && id <= cursor) continue;
    if (rangeStart != -1 && (id < rangeStart || id > rangeEnd)) continue;
    if (count >= limit) break;

    JsonDocument doc;
    recordToJson(rec, doc);

    if (!first) response += ",";
    String out;
    serializeJson(doc, out);
    response += out;

    first = false;
    count++;
  }

  file.close();
  response += "]";
  sendWithCORS(ctx, 200, "application/json", response);
}

// ---------- DELETE ----------
void handleDelete(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  String reqId = ctx.arg("id");
  if (!reqId.length())
    return sendWithCORS(ctx, 400, "text/plain", "Missing id");

  int startId = -1, endId = -1;
  if (reqId.indexOf('-') != -1) {
    int dash = reqId.indexOf('-');
    startId = reqId.substring(0, dash).toInt();
    endId   = reqId.substring(dash + 1).toInt();
  } else {
    startId = endId = reqId.toInt();
  }

  File file = LittleFS.open(FILE_PATH, "r");
  File temp = LittleFS.open("/temp.bin", "w");
  if (!file || !temp)
    return sendWithCORS(ctx, 500, "text/plain", "File error");

  while (file.available()) {
    Record rec;
    file.read((uint8_t*)&rec, sizeof(Record));

    if (rec.id < startId || rec.id > endId)
      temp.write((uint8_t*)&rec, sizeof(Record));
  }

  file.close();
  temp.close();

  LittleFS.remove(FILE_PATH);
  LittleFS.rename("/temp.bin", FILE_PATH);

  sendWithCORS(ctx, 200, "text/plain", "Deleted");
}

// ---------- UPDATE ----------
void handleUpdate(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  if (!ctx.hasArg("id") || !ctx.hasArg("plain"))
    return sendWithCORS(ctx, 400, "text/plain", "Missing id/body");

  int targetId = ctx.arg("id").toInt();

  JsonDocument newDoc;
  if (deserializeJson(newDoc, ctx.arg("plain")))
    return sendWithCORS(ctx, 400, "text/plain", "Invalid JSON");

  File file = LittleFS.open(FILE_PATH, "r");
  File temp = LittleFS.open("/temp.bin", "w");
  if (!file || !temp)
    return sendWithCORS(ctx, 500, "text/plain", "File error");

  bool updated = false;
  while (file.available()) {
    Record rec;
    file.read((uint8_t*)&rec, sizeof(Record));

    if (rec.id == targetId) {
      jsonToRecord(newDoc.as<JsonObject>(), rec, targetId);
      updated = true;
    }

    temp.write((uint8_t*)&rec, sizeof(Record));
  }

  file.close();
  temp.close();

  LittleFS.remove(FILE_PATH);
  LittleFS.rename("/temp.bin", FILE_PATH);

  if (updated)
    sendWithCORS(ctx, 200, "text/plain", "Updated");
  else
    sendWithCORS(ctx, 404, "text/plain", "Not found");
}

// ---------- BULK CREATE ----------
void handleBulkCreate(WebContext &ctx) {

  if (!isAuthorized(ctx)) return;

  if (!ctx.hasArg("plain"))
    return sendWithCORS(ctx, 400, "text/plain", "No body");

  JsonDocument doc;
  if (deserializeJson(doc, ctx.arg("plain")) || !doc.is<JsonArray>())
    return sendWithCORS(ctx, 400, "text/plain", "Invalid JSON array");

  JsonArray arr = doc.as<JsonArray>();
  int count = arr.size();
  if (count == 0)
    return sendWithCORS(ctx, 400, "text/plain", "Empty array");

  // Read + increment ID counter once for the entire batch
  int startId = 1;
  File mfile = LittleFS.open(META_PATH, "r");
  if (mfile) { startId = mfile.readString().toInt(); mfile.close(); }
  File wfile = LittleFS.open(META_PATH, "w");
  if (wfile) { wfile.print(startId + count); wfile.close(); }

  File file = LittleFS.open(FILE_PATH, "a");
  if (!file)
    return sendWithCORS(ctx, 500, "text/plain", "File error");

  int idx = 0;
  for (JsonObject obj : arr) {
    Record rec;
    jsonToRecord(obj, rec, startId + idx);
    file.write((uint8_t*)&rec, sizeof(Record));
    idx++;
  }

  file.close();
  cleanupIfNeeded();

  sendWithCORS(ctx, 200, "text/plain", "Inserted: " + String(count));
}