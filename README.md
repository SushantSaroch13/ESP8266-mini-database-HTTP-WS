# ESP8266 Sensor Database — Full Stack IoT System

A complete IoT pipeline that stores sensor data on an ESP8266's flash memory and exposes a REST API, accessible globally through a WebSocket relay server — no cloud database needed.

```
[ESP8266 Device]  <──WebSocket──>  [Railway Relay]  <──HTTPS──>  [Next.js Dashboard on Vercel]
  LittleFS binary                   Node.js server                  esp-dashboard
  HTTP server :8080                  WebSocket /ws
  Static IP: local                   Public URL
```

---

## Repositories

| Repo | Description |
|------|-------------|
| This repo | ESP8266 firmware (PlatformIO / Arduino) |
| `esp-relay` | Node.js WebSocket relay server (deploy to Railway) |
| `esp-dashboard` | Next.js frontend dashboard (deploy to Vercel) |

---

## Hardware

- **ESP-01 (1MB flash)** running ESP8266 at 80MHz
- Connects to home WiFi with a static local IP
- Stores records as packed binary structs in LittleFS

---

## ESP8266 Firmware

### Architecture

```
HTTP_Server.cpp   — WiFi setup, local HTTP server (:8080), WebSocket relay client
handlers.cpp      — CRUD route handlers (shared by both local HTTP and relay)
web_context.h     — WebContext interface: ServerContext (local) + TunnelContext (relay)
auth.h            — HTTP Basic Auth (Base64)
binary_utils.h    — Record struct + JSON ↔ binary conversion
config.h          — Credentials and network config (not committed — see config.h.example)
```

### Data model

Each record is a fixed-size binary struct stored in `/data.bin`:

```cpp
struct Record {
  char device[10];
  int  temperature;
  int  humidity;
  int  pressure;
  int  id;          // auto-increment
};
```

Auto-incremented IDs are stored in `/meta.txt`. When flash is nearly full, the oldest 20 records are pruned automatically.

### REST API

All endpoints require HTTP Basic Auth: `Authorization: Basic <base64(user:pass)>`

| Method | Path | Query params | Description |
|--------|------|-------------|-------------|
| `GET` | `/data` | `limit`, `cursor`, `id` or `id=start-end` | Read records (newest first) |
| `POST` | `/data` | — | Create one record (JSON body) |
| `POST` | `/data` | — | Bulk create (JSON array body) |
| `PUT` | `/data` | `id` | Update a record (JSON body) |
| `DELETE` | `/data` | `id` or `id=start-end` | Delete record(s) |
| `GET` | `/space` | — | Flash usage stats |

Example request body:
```json
{ "device": "sensor1", "temperature": 25, "humidity": 60, "pressure": 1013 }
```

### Global access — how it works

The ESP is behind a home router with ISP-level CGNAT (port forwarding doesn't work). Instead:

1. The ESP connects **outbound** to a WebSocket relay server on Railway
2. The relay forwards incoming HTTP requests to the ESP over the WebSocket
3. The ESP processes the request using the same handler code and sends the response back
4. No dedicated device needed — the relay runs 24/7 on Railway's free tier

DuckDNS keeps a free public hostname pointing to your home's public IP (used as a fallback for local network access).

---

## Setup

### 1. Clone and configure

```bash
git clone <this-repo>
cp src/config.h.example src/config.h
```

Edit `src/config.h` and fill in:

| Field | Description |
|-------|-------------|
| `BASIC_USER` / `BASIC_PASS` | API credentials |
| `SSID` / `PASS` | WiFi credentials |
| `STATIC_IP` | Fixed local IP for the ESP (outside your DHCP range) |
| `GATEWAY_IP` | Your router's IP |
| `DUCKDNS_DOMAIN` / `DUCKDNS_TOKEN` | From [duckdns.org](https://www.duckdns.org) (free) |
| `RELAY_HOST` | Railway app domain after deploying `esp-relay` |

### 2. Router setup

- Set your router's **DHCP End IP** to leave room for the static IP  
  e.g. DHCP range `.2–.200`, static ESP IP = `.201`
- No port forwarding needed (the relay handles global access)

### 3. Deploy the relay server

```bash
cd esp-relay
# Push to GitHub, then connect to Railway
# Railway auto-detects Node.js and runs `npm start`
# Copy the generated domain into config.h as RELAY_HOST
```

### 4. Flash the ESP

```bash
# Install PlatformIO, then:
pio run --target upload
pio run --target uploadfs   # uploads LittleFS filesystem
```

Serial monitor should show:
```
Connected!
Local IP: 192.168.1.201
Local server on :8080
DuckDNS: OK
Relay: connected
```

### 5. Dashboard

The `esp-dashboard` Next.js app is deployed on Vercel and reads data from the relay URL:

```
https://your-relay.up.railway.app/data?cursor=0&limit=10
Authorization: Basic <base64(user:pass)>
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| `bblanchon/ArduinoJson@7.4.0` | JSON serialization |
| `links2004/WebSockets@^2.6.1` | WebSocket client (relay connection) |
| ESP8266 Arduino core (built-in) | WiFi, HTTP server, LittleFS, HTTPClient |

---

## Project structure

```
src/
├── HTTP_Server.cpp     Main entry point — setup(), loop(), WebSocket relay
├── handlers.cpp        CRUD handlers for all API routes
├── web_context.h       WebContext interface (ServerContext + TunnelContext)
├── handlers.h          Handler declarations
├── auth.h              Basic Auth logic
├── binary_utils.h      Record struct + JSON ↔ binary helpers
├── file_utils.h        LittleFS read/write helpers
├── config.h            Credentials (gitignored)
└── config.h.example    Template — copy to config.h
platformio.ini          PlatformIO project config
```
