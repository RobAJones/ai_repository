# ESP32-S3 AI CAM - Camera Test Page

This Arduino sketch provides a full-featured web interface for the DFRobot ESP32-S3 AI CAM (SKU: DFR1154) with image capture and review capabilities.

## Features

- 📸 **Live Camera Stream** - Real-time video preview in your web browser
- 💾 **SD Card Storage** - Automatically saves captured images to SD card
- 🖼️ **Image Gallery** - View all captured images in a responsive grid
- 👁️ **Image Review** - Click to view full-size images
- 📱 **Responsive Design** - Works on desktop and mobile devices
- 🎨 **Beautiful UI** - Modern gradient design with smooth animations
- 🧠 **Smart PSRAM Detection** - Automatically optimizes quality based on available memory
- ⚡ **Brownout Protection** - Prevents camera initialization failures
- 🎯 **Adaptive Quality** - Uses 800x600 with PSRAM, 640x480 without

## Hardware Requirements

- DFRobot ESP32-S3 AI CAM (DFR1154)
- MicroSD card (formatted as FAT32)
- USB-C cable for programming
- WiFi network

## Software Requirements

- Arduino IDE 2.0 or later
- ESP32 board support package
- Required libraries (install via Arduino Library Manager):
  - ESP32 Camera library (usually included with ESP32 board package)

## Installation

### 1. Install ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. Add this URL to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp32" and install "esp32 by Espressif Systems"

### 2. Configure the Sketch

1. Open `ESP32_S3_Camera_Test.ino` in Arduino IDE
2. Update WiFi credentials at the top of the file:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```

### 3. Board Settings

Configure Arduino IDE with these settings:
- **Board**: "ESP32S3 Dev Module"
- **USB CDC On Boot**: "Enabled"
- **CPU Frequency**: "240MHz (WiFi)"
- **Flash Mode**: "QIO 80MHz"
- **Flash Size**: "8MB (64Mb)"
- **Partition Scheme**: "8M with spiffs (3MB APP/1.5MB SPIFFS)"
- **PSRAM**: "OPI PSRAM"
- **Upload Speed**: "921600"

### 4. Upload

1. Connect your ESP32-S3 AI CAM via USB-C
2. Select the correct port from **Tools → Port**
3. Click **Upload**
4. Wait for compilation and upload to complete

## Usage

### First Time Setup

1. Insert a formatted microSD card into the camera
2. Power on the device
3. Open Serial Monitor (115200 baud) to see the camera's IP address
4. The output will show something like:
   ```
   WiFi connected!
   Camera URL: http://192.168.1.100
   ```

### Accessing the Web Interface

1. Open a web browser (Chrome, Firefox, Safari, etc.)
2. Navigate to the IP address shown in Serial Monitor
3. You should see the camera interface with live stream

### Taking Pictures

1. **Capture Photo**: Click the "📸 CAPTURE PHOTO" button
   - Image is automatically saved to SD card as `/images/IMG_X.jpg`
   - Success message appears showing filename
   - Gallery automatically refreshes

2. **Review Latest**: Click the "👁️ REVIEW LATEST" button
   - Opens the most recently captured image in full-screen view
   - Click anywhere to close

3. **Refresh Gallery**: Click the "🔄 REFRESH GALLERY" button
   - Reloads the gallery grid with all saved images
   - Shows total image count

### Viewing Images

- **Gallery Grid**: All captured images appear in a responsive grid
- **Click any thumbnail**: Opens full-size view in modal
- **Click outside image**: Closes the modal view

## SD Card Structure

Images are saved in this structure:
```
/sdcard
└── /images
    ├── IMG_1.jpg
    ├── IMG_2.jpg
    └── IMG_3.jpg
```

## Troubleshooting

### Camera Not Initializing
- Check that all camera pins are correctly defined
- Verify PSRAM is enabled in board settings
- Try different frame sizes if getting errors

### SD Card Not Mounting
- Ensure SD card is formatted as FAT32
- Check that card is properly inserted
- Try a different SD card (some cards are incompatible)
- Maximum supported size is usually 32GB

### WiFi Connection Issues
- Double-check SSID and password
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Check signal strength in the location

### Web Page Not Loading
- Verify IP address from Serial Monitor
- Ensure computer is on same network
- Try accessing from different browser
- Clear browser cache

### Images Not Saving
- Check SD card has free space
- Verify `/images` directory was created
- Check Serial Monitor for error messages

### Stream Freezing
- Refresh the web page
- Check WiFi signal strength
- Reduce frame size in camera configuration if needed

## Customization

### Change Image Quality

The sketch automatically detects PSRAM and adjusts settings. To manually override:

```cpp
// For boards WITH PSRAM (better quality)
config.frame_size = FRAMESIZE_SVGA;  // 800x600
config.jpeg_quality = 8;              // 0-63, lower is better quality
config.fb_location = CAMERA_FB_IN_PSRAM;
config.grab_mode = CAMERA_GRAB_LATEST;

// For boards WITHOUT PSRAM (conservative)
config.frame_size = FRAMESIZE_VGA;   // 640x480
config.jpeg_quality = 12;
config.fb_location = CAMERA_FB_IN_DRAM;
```

Available frame sizes:
- `FRAMESIZE_UXGA` (1600x1200) - Requires PSRAM
- `FRAMESIZE_SXGA` (1280x1024) - Requires PSRAM
- `FRAMESIZE_XGA` (1024x768) - Requires PSRAM
- `FRAMESIZE_SVGA` (800x600) - Default with PSRAM
- `FRAMESIZE_VGA` (640x480) - Default without PSRAM
- `FRAMESIZE_QVGA` (320x240) - Low resolution

### Adjust Camera Orientation

Modify these lines in the sensor configuration:
```cpp
s->set_vflip(s, 1);    // Vertical flip: 0=no, 1=yes
s->set_hmirror(s, 0);  // Horizontal mirror: 0=no, 1=yes
```

### Change Stream Refresh Rate

Modify this line in the JavaScript:
```javascript
}, 100); // Refresh every 100ms
```

### Customize Colors

Edit the CSS gradient colors in the `<style>` section:
```css
background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
```

## Technical Specifications

- **Resolution**: 
  - With PSRAM: 800x600 (SVGA) at quality level 8
  - Without PSRAM: 640x480 (VGA) at quality level 12
- **Image Format**: JPEG
- **Storage**: MicroSD card (FAT32) via SD_MMC interface
  - CMD (MO): GPIO 11
  - CLK (SCLK): GPIO 12
  - D0 (MI): GPIO 13
- **Memory**: Automatic PSRAM detection and optimization
- **Connectivity**: WiFi 2.4GHz
- **Interface**: Web-based (HTTP)
- **Stream FPS**: ~10fps (configurable)
- **Brownout Protection**: Enabled for stable camera operation

## API Endpoints

The sketch provides these HTTP endpoints:

- `GET /` - Main web interface
- `GET /stream` - JPEG camera stream
- `GET /capture` - Capture and save image (returns JSON)
- `GET /image?file=<path>` - Retrieve saved image
- `GET /list` - List all saved images (returns JSON)
- `GET /latest` - Get latest image filename (returns JSON)

## License

This project is open source and available for personal and educational use.

## Support

For issues with the DFRobot ESP32-S3 AI CAM hardware, refer to:
- [Official Wiki](https://wiki.dfrobot.com/SKU_DFR1154_ESP32_S3_AI_CAM)
- DFRobot Support Forums

## Credits

Created for the DFRobot ESP32-S3 AI CAM (SKU: DFR1154)
