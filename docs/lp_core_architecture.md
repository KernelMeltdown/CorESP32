# LP CORE ARCHITECTURE - BACKGROUND TASK ENGINE
## Autonomous Iteration 4 | Ultra-Low-Power Computing

---

## 🧠 LP CORE CAPABILITIES (from Datasheet Deep-Dive)

**Hardware Specs:**
```
ESP32-C6 LP Core:
├─ Architecture: RISC-V 32-bit
├─ Extensions: IMAC (Integer, Multiply, Atomic, Compressed)
├─ Clock: Up to 20 MHz (vs HP Core: 160 MHz)
├─ SRAM: 16 KB dedicated (LP SRAM)
├─ Power: ~50 µA active (vs HP Core: ~80 mA = 1600x!)
├─ Peripherals Access:
│  ├─ GPIO (all pins via LP IO MUX)
│  ├─ I2C (LP I2C controller)
│  ├─ ADC (can trigger conversions)
│  ├─ RTC (timekeeping)
│  ├─ Timer (LP Timer)
│  └─ IPC with HP Core (mailbox)
└─ Continues running in Deep-Sleep!
```

**CRITICAL INSIGHT:**
```
LP Core ist NICHT "downgraded HP Core"!
LP Core ist SEPARATE CPU die PARALLEL läuft!

┌─────────────────┬─────────────────┐
│   HP Core       │    LP Core      │
│  (160 MHz)      │   (20 MHz)      │
│  ~80 mA         │   ~50 µA        │
├─────────────────┼─────────────────┤
│ Complex Logic   │ Simple Tasks    │
│ Stapeldateien   │ GPIO Polling    │
│ Network         │ Sensor Reading  │
│ Filesystem      │ Wake Decision   │
│ Display (Heavy) │ Display Refresh │
└─────────────────┴─────────────────┘

STRATEGY: HP Core schläft, LP Core übernimmt!
```

---

## 💡 DESIGN PHILOSOPHY: TASK DISTRIBUTION

### Decision Matrix: HP Core vs LP Core vs ETM

| Task Type | ETM | LP Core | HP Core | Reason |
|-----------|-----|---------|---------|--------|
| **LED Blink (Periodic)** | ✅ Best | ⚠️ OK | ❌ Waste | Pure hardware sufficient |
| **GPIO Edge Detection** | ✅ Best | ⚠️ OK | ❌ Waste | Hardware interrupt fastest |
| **Simple GPIO Logic** | ❌ No | ✅ Best | ⚠️ OK | LP Core 1600x more efficient |
| **I2C Sensor Read** | ❌ No | ✅ Best | ⚠️ OK | LP Core has I2C peripheral |
| **ADC with Threshold** | ✅ Best | ⚠️ OK | ❌ Waste | ETM has ADC threshold event |
| **Display Refresh** | ⚠️ Trigger | ✅ Coordinate | ❌ Waste | LP Core + GDMA combo! |
| **Stapeldatei Execution** | ❌ No | ❌ No | ✅ Only | Needs full CPU |
| **Network Protocol** | ❌ No | ❌ No | ✅ Only | Complex, needs HP |
| **Filesystem Access** | ❌ No | ❌ No | ✅ Only | Complex, needs HP |

**Rule of Thumb:**
```
1. Can it be done with ONLY Event → Task? → ETM
2. Needs simple logic but not HP power? → LP Core
3. Complex algorithms or high-level protocols? → HP Core

GOAL: Keep HP Core asleep as much as possible!
```

---

## 📋 LP CORE PROGRAMMING MODEL

### Architecture

```
┌──────────────────────────────────────────┐
│  HP Core (Main Application)              │
│  ├─ Stapeldatei Parser                   │
│  ├─ Network Stack                        │
│  ├─ CoreFS                               │
│  └─ Complex Logic                        │
└──────────────┬───────────────────────────┘
               │ IPC (Mailbox)
┌──────────────▼───────────────────────────┐
│  LP Core (Background Engine)             │
│  ├─ Task 1: Display Refresh (60 FPS)    │
│  ├─ Task 2: Sensor Monitor (1 Hz)       │
│  ├─ Task 3: Wake Decision Logic         │
│  └─ IPC Handler (receive commands)      │
└──────────────────────────────────────────┘
               │
         ┌─────┴─────┐
         │ Hardware  │
         │ ETM+GDMA  │
         └───────────┘
```

---

### LP Core Binary Format

**Toolchain:** RISC-V GCC (from ESP-IDF)

**Binary Structure:**
```
lp_core_binary.bin:
├─ Entry Point: lp_main()
├─ Stack: 2 KB (in LP SRAM)
├─ .text: Code segment
├─ .data: Initialized data
├─ .bss: Uninitialized data
└─ Total: < 16 KB (fits in LP SRAM!)
```

**Loading Process:**
```c
// HP Core loads LP Core binary at boot
#include "lp_core_binary.h"  // Generated from lp_core.c

void load_lp_core(void) {
    // Copy binary to LP SRAM
    memcpy((void*)LP_SRAM_BASE, lp_core_binary, lp_core_binary_size);
    
    // Set entry point
    LP_CORE_ENTRY = LP_SRAM_BASE;
    
    // Start LP Core
    LP_CORE_CTRL |= LP_CORE_START_BIT;
    
    ESP_LOGI(TAG, "LP Core started");
}
```

---

### LP Core Code Example

```c
// lp_core_main.c - Runs on LP Core!

#include "lp_core.h"

// IPC Mailbox (shared memory)
volatile uint32_t* ipc_mailbox = (uint32_t*)0x50000000;

#define IPC_CMD_NONE 0
#define IPC_CMD_REFRESH_DISPLAY 1
#define IPC_CMD_READ_SENSOR 2
#define IPC_CMD_SHUTDOWN 3

// LP Core Entry Point
void lp_main(void) {
    // Initialize LP Peripherals
    lp_gpio_init();
    lp_timer_init();
    lp_i2c_init();
    
    // Main Loop
    while (1) {
        // Check IPC Commands
        uint32_t cmd = ipc_mailbox[0];
        
        switch (cmd) {
            case IPC_CMD_REFRESH_DISPLAY:
                lp_trigger_display_refresh();
                ipc_mailbox[0] = IPC_CMD_NONE;  // Acknowledge
                break;
                
            case IPC_CMD_READ_SENSOR:
                uint16_t value = lp_adc_read(0);
                ipc_mailbox[1] = value;  // Return value
                ipc_mailbox[0] = IPC_CMD_NONE;
                break;
                
            case IPC_CMD_SHUTDOWN:
                return;  // Stop LP Core
                
            default:
                // Background Tasks
                lp_periodic_tasks();
                break;
        }
        
        // Sleep for 10ms
        lp_timer_delay_ms(10);
    }
}

// Periodic Background Tasks (runs every 10ms)
void lp_periodic_tasks(void) {
    static uint32_t counter = 0;
    counter++;
    
    // Every 100 iterations = 1 second
    if (counter % 100 == 0) {
        // Read Temperature Sensor
        uint16_t temp = lp_temp_sensor_read();
        
        // Check Threshold
        if (temp > TEMP_THRESHOLD_HIGH) {
            // Wake HP Core!
            lp_wake_hp_core(WAKE_REASON_TEMP_HIGH);
        }
    }
    
    // Every 1.6 seconds (60 FPS = 16ms, but we're at 10ms intervals)
    // Actually, for 60 FPS we need 16.67ms, but let's do 60 iterations = ~600ms
    // Better: Use LP Timer Interrupt for precise timing!
}

// Trigger Display Refresh (via ETM + GDMA!)
void lp_trigger_display_refresh(void) {
    // Toggle GPIO to trigger ETM
    lp_gpio_set(DISPLAY_TRIGGER_PIN, 1);
    lp_gpio_set(DISPLAY_TRIGGER_PIN, 0);
    
    // ETM routes this to GDMA Start → Display refreshes!
}
```

---

## 🎨 USE CASES

### Use Case 1: 60 FPS Display Refresh (Ultra-Low Power!)

**Goal:** Keep display refreshing at 60 FPS while HP Core sleeps

**Architecture:**
```
LP Core (20 MHz, ~50 µA):
  ├─ Timer Interrupt every 16.67ms
  ├─ Trigger GDMA via ETM (GPIO pulse)
  └─ GDMA sends frame to display (zero CPU!)

HP Core (160 MHz, 0 mA - SLEEPING!):
  └─ Only wakes to update frame buffer (e.g., on user input)

Total Power: ~50 µA (vs 80 mA with HP Core!)
Savings: 1600x Power Reduction!
```

**LP Core Code:**
```c
// lp_core_display.c

#define DISPLAY_FPS 60
#define DISPLAY_INTERVAL_MS (1000 / DISPLAY_FPS)  // 16.67ms

void lp_display_task(void) {
    // Setup Timer for 60 Hz
    lp_timer_config(0, DISPLAY_INTERVAL_MS * 1000);  // µs
    lp_timer_start(0);
    
    while (1) {
        // Wait for timer
        lp_timer_wait(0);
        
        // Trigger GDMA via ETM
        // (GPIO pulse triggers ETM → GDMA Start)
        lp_gpio_set(DISPLAY_TRIGGER_PIN, 1);
        lp_gpio_set(DISPLAY_TRIGGER_PIN, 0);
        
        // DONE! GDMA handles rest via hardware
    }
}
```

**Power Analysis:**
```
LP Core: 50 µA (always on)
GDMA: 100 µA (during transfer, ~1ms per frame)
SPI: 500 µA (during transfer)
Total Average: ~150 µA @ 60 FPS!

vs HP Core approach: 80 mA
Savings: 533x Power Reduction!
```

---

### Use Case 2: Sensor Monitoring 24/7

**Goal:** Monitor temperature sensor, wake HP Core only if threshold exceeded

**LP Core Code:**
```c
// lp_core_sensor.c

#define TEMP_THRESHOLD_HIGH 35  // °C
#define TEMP_THRESHOLD_LOW 10   // °C
#define SENSOR_SAMPLE_RATE_MS 1000  // 1 Hz

void lp_sensor_monitor_task(void) {
    lp_timer_config(1, SENSOR_SAMPLE_RATE_MS * 1000);
    lp_timer_start(1);
    
    while (1) {
        lp_timer_wait(1);
        
        // Read Temperature (via I2C or ADC)
        uint16_t temp = lp_temp_sensor_read();
        
        // Check Thresholds
        if (temp > TEMP_THRESHOLD_HIGH) {
            // Wake HP Core
            lp_wake_hp_core(WAKE_REASON_TEMP_HIGH);
            
            // Wait for HP Core to handle
            lp_timer_delay_ms(1000);
        } else if (temp < TEMP_THRESHOLD_LOW) {
            lp_wake_hp_core(WAKE_REASON_TEMP_LOW);
            lp_timer_delay_ms(1000);
        }
        
        // Otherwise, continue monitoring silently
    }
}
```

**Power:**
```
LP Core: 50 µA
Temperature Sensor: 10 µA
Total: 60 µA 24/7!

vs HP Core polling: 8 mA (even with sleep between polls)
Savings: 133x Power Reduction!
```

---

### Use Case 3: Smart Wake-Up Logic

**Goal:** Decide if HP Core should wake up (advanced filtering!)

**Scenario:** Motion Sensor triggers frequently (false positives)

**Traditional:** Every motion → Wake HP Core → Check → Sleep (wasteful!)

**Smart LP Core:**
```c
// lp_core_motion.c

#define MOTION_DEBOUNCE_MS 100
#define MOTION_COUNT_THRESHOLD 3  // Need 3 detections in 1 second

void lp_motion_monitor_task(void) {
    uint32_t motion_count = 0;
    uint32_t last_motion_time = 0;
    
    while (1) {
        // Wait for GPIO interrupt (motion sensor)
        lp_gpio_wait_interrupt(MOTION_PIN);
        
        uint32_t now = lp_timer_get_ms();
        
        // Debounce
        if (now - last_motion_time > MOTION_DEBOUNCE_MS) {
            motion_count++;
            last_motion_time = now;
        }
        
        // Check if 3 detections within 1 second
        if (motion_count >= MOTION_COUNT_THRESHOLD) {
            // Real motion detected!
            lp_wake_hp_core(WAKE_REASON_MOTION);
            motion_count = 0;  // Reset
        }
        
        // Reset count after 1 second
        if (now - last_motion_time > 1000) {
            motion_count = 0;
        }
    }
}
```

**Result:** HP Core only wakes for REAL motion, not false positives!

---

## 🔧 IPC MECHANISM (HP ↔ LP Communication)

### Mailbox Architecture

```
┌─────────────────────────────────────┐
│  HP Core (160 MHz)                  │
│  ├─ Write to Mailbox: Commands     │
│  └─ Read from Mailbox: Results     │
└──────────────┬──────────────────────┘
               │
        ┌──────▼──────┐
        │  LP SRAM    │  ← Shared Memory!
        │  Mailbox    │
        │  0x50000000 │
        └──────┬──────┘
               │
┌──────────────▼──────────────────────┐
│  LP Core (20 MHz)                   │
│  ├─ Read from Mailbox: Commands    │
│  └─ Write to Mailbox: Results      │
└─────────────────────────────────────┘
```

### Mailbox Protocol

```c
// Shared Header (both HP and LP)
typedef struct {
    volatile uint32_t command;   // Command ID
    volatile uint32_t arg1;      // Argument 1
    volatile uint32_t arg2;      // Argument 2
    volatile uint32_t result;    // Return value
    volatile uint32_t status;    // Status flags
} lp_mailbox_t;

#define LP_MAILBOX ((lp_mailbox_t*)0x50000000)

// Commands (HP → LP)
#define LP_CMD_NONE 0
#define LP_CMD_READ_SENSOR 1
#define LP_CMD_REFRESH_DISPLAY 2
#define LP_CMD_SET_GPIO 3
#define LP_CMD_READ_GPIO 4
#define LP_CMD_I2C_READ 5
#define LP_CMD_I2C_WRITE 6

// Status Flags
#define LP_STATUS_IDLE 0
#define LP_STATUS_BUSY 1
#define LP_STATUS_DONE 2
#define LP_STATUS_ERROR 3
```

### HP Core API

```c
// hp_lp_ipc.c - HP Core side

// Send Command to LP Core
esp_err_t lp_ipc_send(uint32_t cmd, uint32_t arg1, uint32_t arg2) {
    // Wait if LP Core busy
    while (LP_MAILBOX->status == LP_STATUS_BUSY) {
        vTaskDelay(1);
    }
    
    // Write command
    LP_MAILBOX->arg1 = arg1;
    LP_MAILBOX->arg2 = arg2;
    LP_MAILBOX->status = LP_STATUS_IDLE;
    LP_MAILBOX->command = cmd;
    
    return ESP_OK;
}

// Wait for LP Core Result
uint32_t lp_ipc_recv(uint32_t timeout_ms) {
    uint32_t start = xTaskGetTickCount();
    
    while (LP_MAILBOX->status != LP_STATUS_DONE) {
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(timeout_ms)) {
            return 0;  // Timeout
        }
        vTaskDelay(1);
    }
    
    uint32_t result = LP_MAILBOX->result;
    LP_MAILBOX->status = LP_STATUS_IDLE;
    return result;
}

// High-Level Wrappers
uint16_t lp_sensor_read(void) {
    lp_ipc_send(LP_CMD_READ_SENSOR, 0, 0);
    return (uint16_t)lp_ipc_recv(100);
}

void lp_gpio_set_remote(uint8_t pin, uint8_t level) {
    lp_ipc_send(LP_CMD_SET_GPIO, pin, level);
}
```

### LP Core Handler

```c
// lp_core_ipc.c - LP Core side

void lp_ipc_handler(void) {
    uint32_t cmd = LP_MAILBOX->command;
    
    if (cmd == LP_CMD_NONE) return;
    
    LP_MAILBOX->status = LP_STATUS_BUSY;
    
    switch (cmd) {
        case LP_CMD_READ_SENSOR:
            LP_MAILBOX->result = lp_adc_read(0);
            break;
            
        case LP_CMD_SET_GPIO:
            lp_gpio_set(LP_MAILBOX->arg1, LP_MAILBOX->arg2);
            LP_MAILBOX->result = 0;
            break;
            
        case LP_CMD_READ_GPIO:
            LP_MAILBOX->result = lp_gpio_get(LP_MAILBOX->arg1);
            break;
            
        case LP_CMD_I2C_READ: {
            uint8_t addr = LP_MAILBOX->arg1;
            uint8_t reg = LP_MAILBOX->arg2;
            LP_MAILBOX->result = lp_i2c_read_byte(addr, reg);
            break;
        }
            
        // ... more commands
            
        default:
            LP_MAILBOX->status = LP_STATUS_ERROR;
            LP_MAILBOX->command = LP_CMD_NONE;
            return;
    }
    
    LP_MAILBOX->status = LP_STATUS_DONE;
    LP_MAILBOX->command = LP_CMD_NONE;
}
```

---

## 📋 NEW GRUNDBEFEHLE (LP Core Category)

### Command 1: `lp_start`

**Purpose:** Start LP Core with Task

**Syntax:**
```bash
lp_start task=<task_name>
```

**Example:**
```bash
lp_start task=display_refresh

# Loads and starts lp_core_display_refresh.bin
```

---

### Command 2: `lp_stop`

**Purpose:** Stop LP Core

```bash
lp_stop
```

---

### Command 3: `lp_send`

**Purpose:** Send IPC Command to LP Core

**Syntax:**
```bash
lp_send cmd=<command> [arg1=<val>] [arg2=<val>]
```

**Example:**
```bash
# Read Sensor
$value = lp_send cmd=read_sensor

# Set GPIO
lp_send cmd=set_gpio arg1=2 arg2=1
```

---

### Command 4: `lp_status`

**Purpose:** Check LP Core Status

```bash
lp_status

# Output:
# LP Core: RUNNING
# Task: display_refresh
# Uptime: 12345 ms
# Power: 50 µA
```

---

## 🎯 INTEGRATION STRATEGY

### Stapeldatei with LP Core

```bash
# /stapel/power_efficient_display.csp

# Start LP Core for Display Refresh
lp_start task=display_refresh

# HP Core can now sleep!
hp_sleep mode=light duration=10000ms

# After 10 seconds, HP Core wakes up
echo "HP Core awake, LP Core still running display!"

# Update display content
display_load_frame file=/anim/frame1.raw

# LP Core continues refreshing automatically
delay_ms 5000

# Stop LP Core when done
lp_stop

echo "LP Core stopped"
```

---

## ⚖️ BALANCING ACT: ETM vs LP Core vs HP Core

### Decision Tree

```
TASK: Need to do X

Q1: Is it pure Event → Task (no logic)?
    YES → Use ETM (e.g., Button → DMA)
    NO → Continue

Q2: Is it simple logic + low-level peripheral?
    YES → Q3
    NO → HP Core (e.g., Network, Filesystem)

Q3: Needs to run continuously (>1 minute)?
    YES → LP Core (e.g., Display Refresh)
    NO → Q4

Q4: HP Core will be awake anyway?
    YES → HP Core (no point using LP)
    NO → LP Core (save power!)
```

**Examples:**
```
LED Blink (periodic) → ETM (Timer → GPIO)
LED Blink (conditional) → LP Core (if GPIO logic) or HP Core (if complex)
Display Refresh → LP Core + ETM + GDMA (combo!)
Sensor Read (once) → HP Core (simple task)
Sensor Monitor (continuous) → LP Core (power efficient)
Network Request → HP Core (only option)
File Write → HP Core (only option)
Button → Display → ETM → GDMA (no CPU!)
Button → Complex Logic → HP Core (needs processing)
```

---

## 📊 POWER ANALYSIS (Complete System)

### Scenario 1: Idle System (Display Off, No Tasks)

```
ESP32-C6 Deep-Sleep:
├─ HP Core: OFF (0 mA)
├─ LP Core: OFF (0 mA)
├─ RTC: 5 µA
└─ Total: 5 µA
```

### Scenario 2: Display Active (60 FPS), No User Interaction

```
Traditional (HP Core):
├─ HP Core: 80 mA (continuous)
├─ Display: 20 mA (backlight)
├─ Total: 100 mA

CorESP32 (LP Core + ETM + GDMA):
├─ LP Core: 50 µA (timer + coordination)
├─ GDMA: 100 µA (average, during transfers)
├─ SPI: 500 µA (average)
├─ Display: 20 mA (backlight)
├─ HP Core: 0 mA (SLEEPING!)
└─ Total: 20.65 mA

Savings: 100 mA → 20.65 mA = 79% Power Reduction!
```

### Scenario 3: Sensor Monitoring 24/7

```
Traditional:
├─ HP Core: 8 mA (light sleep + periodic wake)
├─ Sensor: 10 µA
└─ Total: 8.01 mA

CorESP32 (LP Core):
├─ LP Core: 50 µA
├─ Sensor: 10 µA
└─ Total: 60 µA

Savings: 8.01 mA → 60 µA = 133x Power Reduction!
```

---

## ✅ IMPLEMENTATION CHECKLIST

### Phase 1: LP Core Infrastructure

```
□ LP Core Toolchain setup (RISC-V GCC)
□ LP SRAM memory map documented
□ LP Core binary loading mechanism
□ IPC Mailbox protocol implemented
□ Basic LP Core test (LED blink) working
```

### Phase 2: LP Core Peripherals

```
□ LP GPIO API (set, get, interrupt)
□ LP Timer API (config, start, wait)
□ LP I2C API (init, read, write)
□ LP ADC API (read, threshold)
□ LP Temp Sensor API
```

### Phase 3: Integration

```
□ HP → LP IPC commands working
□ LP → HP wake-up working
□ LP Core + ETM coordination tested
□ LP Core + GDMA coordination tested
□ Power measurements confirm savings
```

### Phase 4: High-Level Tasks

```
□ Display Refresh Task (60 FPS)
□ Sensor Monitor Task (configurable rate)
□ Wake-up Decision Logic Task
□ Grundbefehle (lp_start, lp_send, etc.)
□ Stapeldatei integration
```

---

## 🎯 NEXT ITERATION PROMPT

**Question:** "Wie fixe ich den CoreFS B-Tree Bug KONKRET? Step-by-Step Implementation!"

**Preview:**
```
Problem: B-Tree nur im RAM
Solution: 
  1. corefs_btree_init() → Write Root Node to Block 1
  2. corefs_btree_load() → Read Root Node from Block 1
  3. corefs_btree_insert() → Write Node back after modify
  4. Test: Create → Reboot → Read muss funktionieren!
```

**Continue to Iteration 5...**

---

**END OF ITERATION 4**

*[Autonomous loop continues...]*
