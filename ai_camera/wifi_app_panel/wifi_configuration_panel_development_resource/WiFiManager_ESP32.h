/*
 * WiFiManager_ESP32.h
 * 
 * Reusable WiFi Configuration Manager for ESP32
 * 
 * Features:
 * - Automatic WiFi configuration portal on first boot
 * - Support for Static IP and DHCP
 * - Network scanning and selection
 * - Non-volatile configuration storage
 * - Factory reset via button hold
 * - Captive portal (DNS redirect)
 * - Responsive web interface
 * 
 * Author: Robert
 * Version: 1.0
 * Date: January 2026
 */

#ifndef WIFIMANAGER_ESP32_H
#define WIFIMANAGER_ESP32_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// =============================================================================
// CONFIGURATION CONSTANTS
// =============================================================================

// WiFi Configuration Portal Settings
#define WM_AP_SSID "ESP32-Setup"           // Access Point name (customize per project)
#define WM_AP_PASSWORD ""                   // Access Point password (empty = open)
#define WM_DNS_PORT 53                      // DNS server port for captive portal
#define WM_WIFI_TIMEOUT 20000               // WiFi connection timeout (ms)
#define WM_CONFIG_TIMEOUT 300000            // Config mode timeout (5 minutes)

// Factory Reset Button Settings
#define WM_RESET_BUTTON_PIN 0               // GPIO pin for reset button (BOOT button)
#define WM_RESET_HOLD_TIME 3000             // Hold time for factory reset (ms)

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * Network configuration structure
 * Stores all WiFi and IP settings
 */
struct NetworkConfig {
  char ssid[32];              // WiFi network name
  char password[64];          // WiFi password
  bool useStaticIP;           // True = static IP, False = DHCP
  IPAddress staticIP;         // Static IP address
  IPAddress gateway;          // Gateway address
  IPAddress subnet;           // Subnet mask
  IPAddress dns1;             // Primary DNS server
  IPAddress dns2;             // Secondary DNS server
};

// =============================================================================
// GLOBAL OBJECTS (Must be declared in main .ino file)
// =============================================================================

extern WebServer server;
extern DNSServer dnsServer;
extern Preferences preferences;
extern NetworkConfig netConfig;

// Configuration mode state variables
extern bool configMode;
extern unsigned long configStartTime;
extern unsigned long buttonPressStartTime;
extern bool buttonPressed;
extern bool resetTriggered;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

/**
 * Load WiFi configuration from non-volatile storage
 * Call this in setup() before attempting WiFi connection
 */
void loadNetworkConfiguration();

/**
 * Save WiFi configuration to non-volatile storage
 * Automatically called after successful configuration
 */
void saveNetworkConfiguration();

/**
 * Attempt to connect to WiFi using saved credentials
 * Returns: true if connected, false if failed
 */
void connectToWiFi();

/**
 * Start configuration portal mode
 * Creates access point and web server for configuration
 */
void startConfigMode();

/**
 * Perform factory reset - clear all WiFi settings
 * Automatically restarts device in configuration mode
 */
void factoryReset();

/**
 * Check reset button state (call in loop())
 * Triggers factory reset on 3-second hold
 */
void checkResetButton();

/**
 * Setup web server routes for configuration portal
 * Call this before server.begin()
 */
void setupWebServerRoutes();

/**
 * Get configuration portal HTML page
 * Returns: Complete HTML as String
 */
String getConfigHTML();

// Web request handlers (called automatically by setupWebServerRoutes)
void handleConfigRoot();
void handleScan();
void handleSaveNetwork();
void handleNetworkStatus();

#endif // WIFIMANAGER_ESP32_H
