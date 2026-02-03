# Hardware Wiring Guide

## ESP32 Pin Connections

This guide shows how to wire external buttons for WPS and Factory Reset functionality.

---

## Complete Wiring Diagram

```
                            ESP32 DevKit
                    ┌─────────────────────┐
                    │                     │
                    │         USB         │
                    │         ╔═╗         │
                    │         ║ ║         │
                    └─────────╚═╝─────────┘
                            
    WPS Button              Reset Button             LED (Optional)
    GPIO 4                  GPIO 0 (BOOT)            GPIO 2
       │                        │                        │
       │                        │                        │
    ┌──┴──┐                 ┌──┴──┐                 ┌──┴──┐
    │     │                 │     │                 │     │
    ● / ● │                 ● / ● │                 │ LED │
    │     │                 │     │                 │  ↑  │
    └──┬──┘                 └──┬──┘                 └──┬──┘
       │                        │                        │
       │                        │                      ┌─┴─┐
       │                        │                      │ R │ 220Ω
       │                        │                      └─┬─┘
       └────────────────────────┴────────────────────────┘
                               GND
```

---

## Minimal Setup (No External Components)

If you don't want to add external buttons, you can use the built-in BOOT button:

```
         ESP32 DevKit
    ┌─────────────────────┐
    │                     │
    │   [BOOT]    [EN]    │  ← Built-in buttons
    │    GPIO0    Reset   │
    │                     │
    │         USB         │
    │         ╔═╗         │
    │         ║ ║         │
    └─────────╚═╝─────────┘

Functions:
• Short press BOOT (1 sec) → Could be WPS (if programmed)
• Long press BOOT (3 sec)  → Factory Reset
```

---

## WPS Button Only Setup

If you only want WPS functionality:

```
         ESP32                     Button
         
    GPIO 4 ●─────────────────────●/●───── GND
         
         Built-in pull-up resistor
         (no external resistor needed)


Breadboard Layout:
    ┌─────────────────────────────┐
    │  ●  ●  ●  ●  ●  ●  ●  ●  ●  │
    │  ●  ●  ●  ●  ●  ●  ●  ●  ●  │ ← Power Rails
    ├─────────────────────────────┤
    │                             │
    │  ●──●  ●  ●  ●  ●  ●  ●  ● │
    │  │  │                       │
    │  ●  ●  ●  ●  ●  ●  ●  ●  ● │
    │  │  │                       │
    │ ESP32                       │
    │                             │
    │  ●  ●  ●  ●  ●  ●  ●  ●  ● │
    │     │                       │
    │  ●  ●  ●  ●  ●  ●  ●  ●  ● │
    │     │                       │
    └─────┼───────────────────────┘
          │
          └── GPIO 4 to button
```

---

## Dual Button Setup (Recommended)

Both WPS and Reset buttons for full functionality:

```
Step 1: Connect WPS Button
    ESP32 GPIO 4 → Button Terminal 1
    Button Terminal 2 → GND

Step 2: Use Built-in BOOT for Reset
    No wiring needed (GPIO 0 has built-in button)

OR

Step 2: Add External Reset Button
    ESP32 GPIO 0 → Button Terminal 1
    Button Terminal 2 → GND
    
    Note: This parallels built-in BOOT button
```

### Breadboard Layout (Dual Button)

```
                    5V   GND                    ← Power Rails
                    ●●   ●●
                    ││   ││
    ┌───────────────┼┼───┼┼───────────────────┐
    │               ││   ││                   │
    │   WPS         ││   ││        Reset      │
    │   Button      ││   ││        Button     │
    │    ●/●────────┼┼───┼┼──────────●/●      │
    │     │         ││   ││           │       │
    │     │         ││   ││           │       │
    │   ┌─┴────────────────────────┴─┐       │
    │   │      ESP32 DevKit          │       │
    │   │                            │       │
    │   │  GPIO4            GPIO0    │       │
    │   │   ●                ●       │       │
    │   │                            │       │
    │   │   GND              GND     │       │
    │   │   ●                ●       │       │
    │   └──┬─────────────────────┬───┘       │
    │      │                     │           │
    │      └─────────┬───────────┘           │
    │                │                       │
    └────────────────┼───────────────────────┘
                     │
                    GND Rail
```

---

## LED Indicator Setup

Add an LED for visual feedback:

```
ESP32 GPIO 2 → Resistor (220Ω) → LED Anode (+) → LED Cathode (-) → GND

Visual Layout:

    ESP32                              LED
    GPIO 2 ●────┬─────────────────────(+)─┤►├──(-)───── GND
                │                           LED
              ┌─┴─┐
              │ R │  220Ω Resistor
              └───┘

LED Polarity:
    ┌───┐
    │ + │  ← Longer leg (Anode)
    │ │ │
    │►├─┤  ← LED symbol
    │ │ │
    │ - │  ← Shorter leg (Cathode)
    └───┘
```

### Color Recommendations
- **Green LED** - General status
- **Blue LED** - WiFi activity
- **Red LED** - Errors
- **RGB LED** - Multiple states

---

## Complete Setup with All Components

```
                         ESP32 DevKit V1
                  ┌──────────────────────────┐
                  │                          │
    WPS Button    │ GPIO 4              VIN  ├─── 5V
         ●/●──────┤  ●                   ●   │
                  │                          │
    Reset Button  │ GPIO 0 (BOOT)       GND  ├─── GND
         ●/●──────┤  ●                   ●   │
                  │                          │
    LED ──────────┤ GPIO 2              3.3V ├─── 3.3V
    (via 220Ω)    │  ●                   ●   │
                  │                          │
                  │         USB Port         │
                  │          ╔═══╗           │
                  └──────────╚═══╝───────────┘

All Grounds Connected:
    Button 1 GND ──┐
    Button 2 GND ──┼── ESP32 GND ── Power Supply GND
    LED GND     ───┘
```

---

## Alternative GPIO Pins

If GPIO 4 is already in use, you can change the WPS button pin:

### Recommended Alternative GPIOs

**Safe to use (won't interfere with boot):**
- GPIO 15, 16, 17, 18, 19
- GPIO 21, 22, 23
- GPIO 25, 26, 27
- GPIO 32, 33

**Avoid these (used for other functions):**
- GPIO 0 - BOOT button
- GPIO 1, 3 - Serial (USB)
- GPIO 6-11 - Flash memory
- GPIO 12 - Affects boot voltage
- GPIO 34-39 - Input only (no pull-up)

### Code Change for Different Pin

```cpp
// Change this line in the code:
#define WPS_BUTTON_PIN 15  // Changed from 4 to 15

// Then wire button to GPIO 15 instead
```

---

## Button Types & Specifications

### Recommended Buttons

**Tactile Push Buttons:**
- 6x6mm PCB mount buttons
- 12x12mm panel mount buttons
- Momentary contact (normally open)
- Rating: 12V, 50mA minimum

**Example Part Numbers:**
- Omron B3F series
- Alps SKQG series
- TE Connectivity FSM series

### Button Specifications

```
Mechanical:
• Type: SPST (Single Pole Single Throw)
• Action: Momentary (spring return)
• Contact: Normally Open (N.O.)

Electrical:
• Voltage: 12V DC minimum
• Current: 50mA minimum
• Life: 100,000 operations minimum

Physical:
• Size: 6mm to 12mm
• Travel: 0.25mm to 0.5mm
• Force: 100gf to 260gf
```

---

## Wiring Tips

### Do's ✅

1. **Keep wires short** - Less than 30cm if possible
2. **Use solid core wire** - 22-24 AWG for breadboard
3. **Strip 5mm of insulation** - Just enough for connection
4. **Test continuity** - Use multimeter before powering
5. **Label wires** - Helps with troubleshooting
6. **Use color coding:**
   - Red = Power (VCC, 3.3V, 5V)
   - Black = Ground (GND)
   - Yellow/Blue = Signals (GPIO)

### Don'ts ❌

1. **Don't cross wires** - Keep layout organized
2. **Don't use long wires** - Can pick up interference
3. **Don't reverse polarity** - Check LED direction
4. **Don't share grounds poorly** - Star ground preferred
5. **Don't forget pull-ups** - Built-in used, but important

### Common Mistakes

**Mistake 1: Wrong Button Type**
```
❌ Using latching/toggle switch
✅ Use momentary push button
```

**Mistake 2: LED No Resistor**
```
❌ GPIO → LED → GND (LED burns out!)
✅ GPIO → Resistor → LED → GND
```

**Mistake 3: Input-Only GPIO**
```
❌ Using GPIO 36 for button (input only, no pull-up)
✅ Use GPIO 4, 15, etc. (have pull-up)
```

---

## Testing Your Wiring

### Button Test

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(WPS_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(WPS_BUTTON_PIN) == LOW) {
    Serial.println("WPS Button Pressed!");
  }
  delay(100);
}
```

### LED Test

```cpp
void setup() {
  pinMode(2, OUTPUT);
}

void loop() {
  digitalWrite(2, HIGH);  // On
  delay(1000);
  digitalWrite(2, LOW);   // Off
  delay(1000);
}
```

### Multimeter Tests

**Button Test:**
1. Set multimeter to continuity/resistance mode
2. Touch probes to button terminals
3. Should read:
   - Open (∞ Ω) when not pressed
   - Closed (0 Ω) when pressed

**Power Test:**
1. Set multimeter to DC voltage mode
2. Measure between VIN and GND
3. Should read:
   - 5V if powered via USB
   - Input voltage if using external supply

---

## Power Considerations

### USB Power (Most Common)
```
USB Cable → ESP32 VIN → Internal regulator → 3.3V

Typical current draw:
• Idle: 80mA
• WiFi active: 160-260mA
• Peak: 500mA (during connection)

USB provides: 500mA (enough for ESP32)
```

### External Power

If adding many components:

```
Power Supply Options:
1. 5V wall adapter → VIN pin
2. 3.7V LiPo battery → VIN pin (with protection)
3. 3.3V regulated → 3.3V pin (bypass regulator)

Minimum ratings:
• Voltage: 5V ±5%
• Current: 1A (with margin)
```

---

## Breadboard Layout Tips

### Good Layout
```
    [Power Rails]
    [ESP32]
    [Buttons]
    [LEDs]
    [Empty Space]
    
    Keep related components together
    Short wires
    Organized
```

### Poor Layout
```
    [ESP32]
    [Long wire]
    [Button]
    [Long wire]
    [LED]
    [Long wire]
    [Power]
    
    Messy
    Long wires
    Hard to debug
```

---

## Enclosure Mounting

### Panel Mount Buttons

```
Front Panel View:
    ┌─────────────────────┐
    │                     │
    │   [WPS]    [Reset]  │  ← Buttons accessible
    │                     │
    │   ┌──────────┐      │
    │   │  ESP32   │      │  ← Inside enclosure
    │   │          │      │
    │   └──────────┘      │
    │                     │
    │     ●  LED          │  ← Status indicator
    └─────────────────────┘

Button labels:
• Engraved
• Printed labels
• 3D printed inserts
```

---

## Troubleshooting Wiring

### Button Not Detected

**Check:**
1. Continuity through button when pressed
2. GPIO pin defined correctly in code
3. Wire not broken
4. Button actually momentary (not latching)

**Test:**
```cpp
Serial.println(digitalRead(WPS_BUTTON_PIN));
// Should print 1 (HIGH) normally
// Should print 0 (LOW) when pressed
```

### LED Not Working

**Check:**
1. LED polarity (+ to resistor, - to GND)
2. Resistor value (220Ω typical)
3. GPIO pin defined correctly
4. LED not burned out (test with multimeter)

**Test:**
```cpp
pinMode(2, OUTPUT);
digitalWrite(2, HIGH);  // LED should light up
```

### ESP32 Not Powering On

**Check:**
1. USB cable has data lines (not charge-only)
2. USB port provides enough current
3. Power supply voltage correct (5V)
4. No short circuits (check with multimeter)

---

## Summary

**Minimum Setup:**
- Use built-in BOOT button for factory reset
- No external components needed

**Recommended Setup:**
- External button on GPIO 4 for WPS
- Built-in BOOT for factory reset
- LED on GPIO 2 for status (optional)

**Maximum Setup:**
- WPS button (GPIO 4)
- Reset button (GPIO 0 external + built-in)
- Status LED (GPIO 2)
- RGB LED for multi-state feedback (optional)

**Key Points:**
- All buttons use INPUT_PULLUP (no external resistor)
- LEDs always need current-limiting resistor (220Ω)
- Keep wires short and organized
- Test with multimeter before powering on

---

**Happy Wiring! 🔌⚡**
