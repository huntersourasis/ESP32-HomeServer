/*
  ESP32 SD Webserver - Pro UI + AP+STA + Fixed Upload Redirect
  -------------------------------------------------------------
  - SoftAP + Station simultaneously (connect to AP or your Home Wi-Fi)
  - mDNS: http://esp32server.local/ (works on many OSes for STA)
  - Upload: first-click works; browser auto-redirects to /files after upload
  - File Manager: list / download / delete with clean, modern styling
  - Serves files from SD root; auto-serves /index.html at "/"
  - Extended MIME types for code/scripts/media/archive

  Hardware: SD in SPI mode (set SD_CS pin as needed)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// ============ WIFI CONFIG ============
const char* STA_SSID = "YOUR_WIFI_SSID";         // your home Wi-Fi
const char* STA_PASS = "PASSWORD";

const char* AP_SSID  = "ESP32-FTP"; // ESP32 hotspot
const char* AP_PASS  = "12345678";     // 8+ chars

const char* MDNS_NAME = "homeserver"; // http://homeserver.local/

// SD card CS pin (adjust if needed)
#define SD_CS 5
// =====================================

WebServer server(80);
File uploadFile;

// ---------------- MIME TYPES ----------------
String getContentType(const String& path) {
  String p = path; p.toLowerCase();

  // Web
  if (p.endsWith(".htm") || p.endsWith(".html")) return "text/html";
  if (p.endsWith(".css")) return "text/css";
  if (p.endsWith(".js")) return "application/javascript";
  if (p.endsWith(".json")) return "application/json";
  if (p.endsWith(".xml")) return "application/xml";
  if (p.endsWith(".txt")) return "text/plain";

  // Images
  if (p.endsWith(".png")) return "image/png";
  if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
  if (p.endsWith(".gif")) return "image/gif";
  if (p.endsWith(".ico")) return "image/x-icon";
  if (p.endsWith(".svg")) return "image/svg+xml";
  if (p.endsWith(".webp")) return "image/webp";

  // Docs / Archives
  if (p.endsWith(".pdf")) return "application/pdf";
  if (p.endsWith(".zip")) return "application/zip";
  if (p.endsWith(".gz"))  return "application/gzip";
  if (p.endsWith(".tar")) return "application/x-tar";
  if (p.endsWith(".rar")) return "application/vnd.rar";
  if (p.endsWith(".7z"))  return "application/x-7z-compressed";

  // Code / Scripts
  if (p.endsWith(".c"))    return "text/x-csrc";
  if (p.endsWith(".cpp"))  return "text/x-c++src";
  if (p.endsWith(".h"))    return "text/x-chdr";
  if (p.endsWith(".hpp"))  return "text/x-c++hdr";
  if (p.endsWith(".ino"))  return "text/x-arduino";
  if (p.endsWith(".py"))   return "text/x-python";
  if (p.endsWith(".sh"))   return "application/x-sh";
  if (p.endsWith(".bat"))  return "application/x-msdos-program";
  if (p.endsWith(".ps1"))  return "application/powershell";

  // Media
  if (p.endsWith(".mp3"))  return "audio/mpeg";
  if (p.endsWith(".wav"))  return "audio/wav";
  if (p.endsWith(".ogg"))  return "audio/ogg";
  if (p.endsWith(".mp4"))  return "video/mp4";

  return "application/octet-stream";
}

// --------------- UI HELPERS -----------------
String pageHead(const String& title) {
  return
    "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>" + title + "</title>"
    "<style>"
    ":root{--bg:#0f1115;--card:#151923;--muted:#9aa3af;--text:#e5e7eb;--acc:#4f46e5;--ok:#22c55e;--warn:#ef4444}"
    "*{box-sizing:border-box}html,body{margin:0;height:100%;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,'Helvetica Neue',Arial}"
    ".wrap{max-width:980px;margin:0 auto;padding:24px}"
    ".nav{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin:8px 0 24px}"
    ".btn{display:inline-flex;align-items:center;gap:8px;padding:10px 14px;border-radius:10px;border:1px solid #22242c;background:#1b2030;color:var(--text);text-decoration:none;cursor:pointer}"
    ".btn:hover{border-color:#2c3140;transform:translateY(-1px)}"
    ".btn.primary{background:var(--acc)}"
    ".btn.ok{background:var(--ok)}"
    ".btn.warn{background:#b91c1c}"
    ".card{background:var(--card);border:1px solid #1d2430;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,.35);padding:20px;margin:0 0 16px}"
    "h1{font-weight:700;letter-spacing:.3px;margin:4px 0 12px;font-size:26px}"
    "h2{font-weight:600;margin:0 0 12px;font-size:20px;color:#c7d2fe}"
    "p{color:var(--muted);margin:6px 0 12px}"
    "table{width:100%;border-collapse:collapse;background:transparent}"
    "th,td{text-align:left;padding:10px;border-bottom:1px solid #222835}"
    "th{color:#cbd5e1;font-weight:600;background:#121621;position:sticky;top:0}"
    "tbody tr:hover{background:#121621}"
    ".actions{display:flex;gap:8px;flex-wrap:wrap}"
    ".tag{display:inline-block;padding:2px 10px;border-radius:999px;background:#0b1020;border:1px solid #22273a;color:#9aa3af;font-size:12px;margin-right:6px}"
    /* Fancy file input */
    ".filebox{display:flex;flex-wrap:wrap;gap:10px;align-items:center;justify-content:center}"
    ".filebox input[type=file]{display:none}"
    ".filebox .choose{padding:10px 14px;border-radius:10px;background:#1b2030;border:1px dashed #2b3142;cursor:pointer}"
    ".filebox .choose:hover{border-style:solid}"
    ".filebox .filename{min-width:200px;padding:10px 12px;border-radius:10px;border:1px solid #212635;background:#0f1422}"
    ".filebox .submit{padding:10px 14px;border-radius:10px;background:var(--acc);border:none;color:#fff;cursor:pointer}"
    ".filebox .submit:disabled{opacity:.6;cursor:not-allowed}"
    ".grid{display:grid;grid-template-columns:1fr;gap:16px}"
    "@media(min-width:720px){.grid{grid-template-columns:1fr 1fr}}"
    "</style></head><body><div class='wrap'>";
}

String pageFootJS() {
  return
    "<script>"
    "document.addEventListener('change',e=>{if(e.target && e.target.id==='file'){"
      "const span=document.getElementById('filename');"
      "span.textContent=e.target.files.length?e.target.files[0].name:'No file chosen';"
      "document.getElementById('submit').disabled=!e.target.files.length;"
    "}});"
    "</script></div></body></html>";
}

String humanSize(uint64_t bytes) {
  const char* units[] = {"B","KB","MB","GB"};
  int u = 0; double sz = bytes;
  while (sz >= 1024 && u < 3) { sz/=1024.0; u++; }
  char buf[32];
  snprintf(buf, sizeof(buf), (sz < 10 && u>0) ? "%.1f %s" : "%.0f %s", sz, units[u]);
  return String(buf);
}

// -------------- FILE SERVING ----------------
void serveFilePath(const String& path) {
  if (!SD.exists(path)) { server.send(404, "text/plain", "File Not Found"); return; }
  File file = SD.open(path, FILE_READ);
  if (!file) { server.send(500, "text/plain", "Open failed"); return; }
  server.streamFile(file, getContentType(path));
  file.close();
}

void handleFileRead(String path) {
  String p = path;
  if (p.endsWith("/")) p += "index.html";
  if (!SD.exists(p)) { server.send(404, "text/plain", "File Not Found"); return; }
  serveFilePath(p);
}

// -------------- PAGES -----------------------
void handleRoot() {
  // If index.html exists, serve the site
  if (SD.exists("/index.html")) { serveFilePath("/index.html"); return; }

  IPAddress apIP = WiFi.softAPIP();
  IPAddress staIP = WiFi.localIP();

  String html = pageHead("ESP32 SD Webserver");
  html +=
    "<div class='grid'>"
      "<div class='card'>"
        "<h1>ESP32 SD Webserver</h1>"
        "<p class='muted'>Host a website from your SD card. Upload files, manage them, and access from your phone or laptop.</p>"
        "<div class='nav'>"
          "<a class='btn primary' href='/upload'>Upload</a>"
          "<a class='btn' href='/files'>File Manager</a>"
        "</div>"
        "<p>"
          "<span class='tag'>AP IP: " + apIP.toString() + "</span>"
          "<span class='tag'>STA IP: " + (staIP == IPAddress((uint32_t)0) ? String("not connected") : staIP.toString()) + "</span>"
          "<span class='tag'>mDNS: " + String(MDNS_NAME) + ".local</span>"
        "</p>"
        "<p class='muted'>Tip: Place <code>index.html</code> in SD root to auto-serve your site at <code>/</code>.</p>"
      "</div>"
      "<div class='card'>"
        "<h2>Quick Links</h2>"
        "<div class='nav'>"
          "<a class='btn ok' href='/files'>Open File Manager</a>"
          "<a class='btn' href='/upload'>Open Uploader</a>"
        "</div>"
        "<p class='muted'>Use the uploader to add files, then manage them in the file manager.</p>"
      "</div>"
    "</div>";
  html += pageFootJS();
  server.send(200, "text/html", html);
}

void handleUploadPage() {
  String html = pageHead("Upload");
  html +=
    "<div class='card'>"
      "<h1>Upload File</h1>"
      "<form method='POST' action='/upload' enctype='multipart/form-data'>"
        "<div class='filebox'>"
          "<label class='choose' for='file'>Choose File</label>"
          "<input id='file' name='data' type='file'>"
          "<span id='filename' class='filename'>No file chosen</span>"
          "<button id='submit' class='submit' type='submit' disabled>Upload</button>"
        "</div>"
      "</form>"
      "<div class='nav' style='margin-top:16px'>"
        "<a class='btn' href='/files'>File Manager</a>"
        "<a class='btn' href='/'>Home</a>"
      "</div>"
    "</div>";
  html += pageFootJS();
  server.send(200, "text/html", html);
}

void handleFileManager() {
  String html = pageHead("File Manager");
  html += "<div class='card'><h1>SD Card Files</h1>"
          "<div class='nav'><a class='btn primary' href='/upload'>Upload</a><a class='btn' href='/'>Home</a></div>"
          "<div style='overflow:auto;border-radius:12px'>"
          "<table><thead><tr><th>Name</th><th>Size</th><th>Actions</th></tr></thead><tbody>";

  File root = SD.open("/");
  if (!root) {
    html += "<tr><td colspan='3'>Failed to open SD root</td></tr>";
  } else {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String fname = String(file.name());
        html += "<tr><td>" + fname + "</td><td>" + humanSize(file.size()) + "</td>"
                "<td class='actions'>"
                  "<a class='btn' href='/download?file=" + fname + "'>Download</a>"
                  "<form method='POST' action='/delete' onsubmit='return confirm(\"Delete " + fname + "?\");' style='display:inline'>"
                  "<input type='hidden' name='file' value='" + fname + "'>"
                  "<button class='btn warn' type='submit'>Delete</button>"
                  "</form>"
                "</td></tr>";
      }
      file = root.openNextFile();
    }
    root.close();
  }
  html += "</tbody></table></div></div>";
  html += pageFootJS();
  server.send(200, "text/html", html);
}

// -------------- SAFETY ----------------------
bool isSafeName(const String& s) {
  return (s.indexOf('/') < 0) && (s.indexOf('\\') < 0) && (s.indexOf("..") < 0);
}

// -------------- DOWNLOAD / DELETE -----------
void handleDownload() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Missing 'file'"); return; }
  String fname = server.arg("file");
  if (!isSafeName(fname)) { server.send(400, "text/plain", "Invalid filename"); return; }
  String path = "/" + fname;
  if (!SD.exists(path)) { server.send(404, "text/plain", "Not found"); return; }
  File file = SD.open(path, FILE_READ);
  if (!file) { server.send(500, "text/plain", "Open failed"); return; }

  server.setContentLength(file.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
  server.streamFile(file, getContentType(path));
  file.close();
}

void handleDelete() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Missing 'file'"); return; }
  String fname = server.arg("file");
  if (!isSafeName(fname)) { server.send(400, "text/plain", "Invalid filename"); return; }
  String path = "/" + fname;
  if (!SD.exists(path)) { server.send(404, "text/plain", "Not found"); return; }
  if (!SD.remove(path)) { server.send(500, "text/plain", "Delete failed"); return; }
  server.sendHeader("Location", "/files");
  server.send(303); // See Other
}

// -------------- UPLOAD (chunked) ------------
void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    if (!filename.startsWith("/")) filename = "/" + filename;

    if (SD.exists(filename)) SD.remove(filename);
    uploadFile = SD.open(filename, FILE_WRITE);
    Serial.printf("Upload start: %s\n", filename.c_str());
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("Upload end: %u bytes\n", upload.totalSize);
    }
    // Note: we DO NOT send a response here. Redirect is sent in the POST completion handler.
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) { uploadFile.close(); }
    Serial.println("Upload aborted");
  }
}

// After upload completes, send a redirect so browser lands on /files automatically
void handleUploadDoneRedirect() {
  server.sendHeader("Location", "/files");
  server.send(303); // See Other
}

// -------------- WIFI ------------------------
void startWiFi() {
  WiFi.mode(WIFI_AP_STA);

  // Start AP
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP IP:   "); Serial.println(WiFi.softAPIP());

  // Connect STA
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Connecting STA to "); Serial.println(STA_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nSTA IP:  "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSTA not connected (timeout). AP still available.");
  }

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("mDNS: http://"); Serial.print(MDNS_NAME); Serial.println(".local/");
  } else {
    Serial.println("mDNS start failed.");
  }
}

// -------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed!");
  } else {
    Serial.println("SD Card Mounted.");
  }

  startWiFi();

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_GET, handleUploadPage);
  // POST completion handler sends redirect; second callback receives data chunks
  server.on("/upload", HTTP_POST, handleUploadDoneRedirect, handleFileUpload);

  server.on("/files", HTTP_GET, handleFileManager);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_POST, handleDelete);

  // Any other path: try to serve from SD
  server.onNotFound([](){ handleFileRead(server.uri()); });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
