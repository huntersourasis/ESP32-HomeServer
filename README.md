# ESP32 Webserver with MicroSD Card Support

This project allows you to host a **webserver on ESP32** that serves files directly from a **MicroSD card module**.  
You can upload, view, download, and delete files from the SD card via the web interface.

---

## ✨ Features
- ESP32 creates a webserver accessible over WiFi.
- Connects automatically to your **home WiFi** (edit SSID & password in code).
- Supports **mDNS** → access your ESP32 at:  
  👉 `http://homeserver.local`
- MicroSD card support for serving static files (HTML, CSS, JS, images, etc.).
- Built-in **file manager**:
  - List files stored on SD card
  - Upload new files
  - Download existing files
  - Delete files

---

## 🛠 Hardware Required
- ESP32 Development Board (ESP32-WROOM-32D recommended)
- MicroSD Card Module (SPI interface)
- MicroSD Card (formatted as FAT32)
- Jumper wires

---

## 🔌 Wiring (ESP32 ↔ MicroSD Card Module)

| MicroSD Pin | ESP32 Pin |
|-------------|-----------|
| **VCC**     | 3.3V      |
| **GND**     | GND       |
| **MOSI**    | GPIO 23   |
| **MISO**    | GPIO 19   |
| **SCK**     | GPIO 18   |
| **CS**      | GPIO 5    |

📷 Wiring diagram included in this repo: `esp32_sdcard_connection.png`

---

## 📂 Project Structure
```
/ESP32-WebServer-SD
│── ESP32_SD_WebServer.ino   # Main Arduino sketch
│── esp32_sdcard_connection.png  # Wiring diagram
│── /data
│    ├── index.html          # Example homepage
│    ├── style.css           # Example stylesheet
│    └── script.js           # Example JavaScript
│── README.md
```

---

## 🚀 How to Use
1. Clone this repository:
   ```bash
   git clone https://github.com/huntersourasis/ESP32-HomeServer.git
   ```
2. Open `ESP32_SD_WebServer.ino` in Arduino IDE or PlatformIO.
3. Update your WiFi **SSID** and **password** in the code:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "PASSWORD";
   ```
4. Upload the code to your ESP32.
5. Insert formatted microSD card into the SD module.
6. Connect to `http://homeserver.local` in your browser.
7. You can also access it by connecting to it :
   ```bash
      SSID = ESP32-FTP
      PASSWORD = "12345678"
   ```

---

## 🌐 Access Points
- `http://homeserver.local/` → Serves `index.html` from SD card
- `http://homeserver.local/upload` → Upload files
- `http://homeserver.local/files` → File manager (list, download, delete)

---

## 📷 Preview
![ESP32 with MicroSD Connection](esp32_sdcard_connection.png)

---

## 🧑‍💻 Author
Developed by **Sourasis Maity (huntersourasis)**  
🔗 [GitHub Profile](https://github.com/huntersourasis)
