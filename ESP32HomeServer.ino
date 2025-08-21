/*
  ESP32 SD Webserver with upload, file manager, download, delete, and mDNS.
  - Connects to WiFi (replace SSID/PASS).
  - mDNS name: http://hunterhome.local/
  - Upload files: /upload
  - File manager: /files  (list / download / delete)
  - Serves files from SD card root.
  - SD card uses SPI mode; change SD_CS pin if needed.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <SPI.h>

const char* ssid     = "YOUR_HOME_WIFI_SSID";
const char* password = "WIFI_PASSOWRD";

WebServer server(80);

// SD card CS pin (adjust if needed)
#define SD_CS 5

// ---------- Utility: detect MIME type ----------
String getContentType(const String& path) {
  if (path.endsWith(".htm") || path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".pdf")) return "application/pdf";
  if (path.endsWith(".txt")) return "text/plain";
  // fallback
  return "application/octet-stream";
}

// ---------- Serve files from SD ----------
void handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  String filepath = path; // e.g. "/index.html" or "/about.html"

  if (!SD.exists(filepath)) {
    server.send(404, "text/plain", "File Not Found");
    return;
  }

  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }

  String contentType = getContentType(filepath);
  server.streamFile(file, contentType);
  file.close();
}

// ---------- Upload handling ----------
File uploadFile;

void handleUploadPage() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>Upload</title></head><body>";
  html += "<h2>Upload File to SD Card</h2>";
  html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='data'><input type='submit' value='Upload'></form>";
  html += "<p><a href='/files'>Open File Manager</a></p>";
  html += "<p><a href='/'>Home</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    // sanitize filename a bit: remove path components
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    String filepath = "/" + filename;
    Serial.printf("Upload Start: %s\n", filepath.c_str());
    if (SD.exists(filepath)) { SD.remove(filepath); } // overwrite
    uploadFile = SD.open(filepath, FILE_WRITE);
    if (!uploadFile) Serial.println("Failed to open file for writing");
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("Upload End: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
      server.sendHeader("Location", "/files", true);
      server.send(302, "text/plain", ""); // redirect to file manager
    } else {
      server.send(500, "text/plain", "Failed to save file");
    }
  }
}

// ---------- File Manager: list files ----------
void handleFileManager() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>SD File Manager</title></head><body>";
  html += "<h2>SD Card File Manager</h2>";
  html += "<p><a href='/upload'>Upload file</a> | <a href='/'>Home</a></p>";
  html += "<table border='1' cellpadding='6' cellspacing='0'><tr><th>Filename</th><th>Size (bytes)</th><th>Actions</th></tr>";

  File root = SD.open("/");
  if (!root) {
    html += "<tr><td colspan='3'>Failed to open SD root</td></tr>";
  } else {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String fname = String("/") + String(file.name());
        size_t fsize = file.size();
        html += "<tr>";
        html += "<td>" + String(file.name()) + "</td>";
        html += "<td>" + String(fsize) + "</td>";

        // Download link
        html += "<td>";
        html += "<a href='/download?file=" + String(file.name()) + "'>Download</a> ";

        // Delete form (simple POST)
        html += "<form style='display:inline' method='POST' action='/delete' onsubmit='return confirm(\"Delete this file?\");'>";
        html += "<input type='hidden' name='file' value='" + String(file.name()) + "'>";
        html += "<input type='submit' value='Delete'>";
        html += "</form>";

        html += "</td>";
        html += "</tr>";
      }
      file = root.openNextFile();
    }
  }
  html += "</table>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ---------- Download handler ----------
void handleDownload() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }
  String fname = server.arg("file");
  // sanitize
  if (fname.indexOf('/') >= 0) {
    server.send(400, "text/plain", "Invalid filename");
    return;
  }
  String filepath = "/" + fname;
  if (!SD.exists(filepath)) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  String contentType = getContentType(filepath);
  server.setContentLength(file.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
  server.streamFile(file, contentType);
  file.close();
}

// ---------- Delete handler ----------
void handleDelete() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }
  String fname = server.arg("file");
  if (fname.indexOf('/') >= 0) {
    server.send(400, "text/plain", "Invalid filename");
    return;
  }
  String filepath = "/" + fname;
  if (!SD.exists(filepath)) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  bool ok = SD.remove(filepath);
  if (ok) {
    server.sendHeader("Location", "/files", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(500, "text/plain", "Failed to delete file");
  }
}

// ---------- Root page (simple) ----------
void handleRoot() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>ESP32 SD Webserver</title></head><body>";
  html += "<h1>ESP32 SD Webserver</h1>";
  html += "<p>mDNS: <strong>http://homeserver.local/</strong></p>";
  html += "<p><a href='/files'>File Manager</a> | <a href='/upload'>Upload</a></p>";
  html += "<hr>";
  html += "<p>To serve your uploaded website files, place an <code>index.html</code> in the SD card root and visit <code>http://homeserver.local/</code></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    // avoid blocking forever — but keep trying
    if (millis() - start > 20000) {
      Serial.println("\nStill trying to connect...");
      start = millis();
    }
  }
  Serial.println("\nConnected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // mDNS
  if (!MDNS.begin("homeserver")) {
    Serial.println("Error starting MDNS");
  } else {
    Serial.println("mDNS responder started: http://homeserver.local/");
  }

  // Init SD Card (SPI)
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed!");
  } else {
    Serial.println("SD Card Mounted");
  }

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_GET, handleUploadPage);
  server.on("/upload", HTTP_POST, [](){ server.send(200); }, handleFileUpload);
  server.on("/files", HTTP_GET, handleFileManager);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_POST, handleDelete);

  // Serve files from SD for any other path
  server.onNotFound([]() {
    String uri = server.uri();
    // If request looks like a file, try to serve it from SD
    handleFileRead(uri);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
