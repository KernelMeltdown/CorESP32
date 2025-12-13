# GDMA COMMAND INTEGRATION - COMPLETE DESIGN
## Autonomous Iteration 2 | Self-Prompting Deep-Dive

---

## 🎯 DESIGN GOALS

```
1. TRANSPARENT: User muss nicht verstehen wie GDMA intern funktioniert
2. POWERFUL: Experten können volle Hardware-Control nutzen
3. SAFE: Keine Buffer Overruns, keine Cache-Bugs
4. COMPOSABLE: GDMA Commands kombinierbar mit anderen Commands
5. EFFICIENT: Zero-Copy wo immer möglich
```

---

## 📋 NEW GRUNDBEFEHLE (DMA Category)

### Command 1: `dma_copy`

**Purpose:** Zero-Copy Memory Transfer

**Syntax:**
```bash
dma_copy src=<source> dst=<dest> size=<bytes> [wait=<true|false>]
```

**Arguments:**
- `src`: Source specification
  - `mem:0xNNNN` - Memory address
  - `file:/path` - File (CoreFS Memory-Mapped!)
  - `gpio:N` - GPIO input (bit-stream)
- `dst`: Destination specification
  - `mem:0xNNNN` - Memory address
  - `spi:N` - SPI peripheral
  - `i2s:N` - I2S peripheral
  - `uart:N` - UART peripheral
  - `file:/path` - File (CoreFS)
- `size`: Bytes to transfer (must be 4-byte aligned!)
- `wait`: Block until complete (default: false)

**Examples:**
```bash
# Copy file to SPI (Display)
dma_copy src=file:/img.raw dst=spi:2 size=153600

# Copy ADC buffer to file
dma_copy src=mem:0x3FC00000 dst=file:/log.bin size=4096

# Copy encrypted file to display (via AES GDMA!)
dma_copy src=file:/img.enc dst=aes:decrypt dst=spi:2 size=153600
```

**Return Value:**
- DMA Handle (for status checking)
- Or -1 on error

---

### Command 2: `dma_chain`

**Purpose:** Multi-Transfer Pipeline (Linked Descriptors!)

**Syntax:**
```bash
dma_chain <desc1> <desc2> <desc3> ... [wait=<bool>]
```

**Each Descriptor:**
```
src=<source> dst=<dest> size=<bytes>
```

**Example:**
```bash
# 4-Frame Display Refresh
dma_chain \
  src=file:/frame1.raw dst=spi:2 size=153600 \
  src=file:/frame2.raw dst=spi:2 size=153600 \
  src=file:/frame3.raw dst=spi:2 size=153600 \
  src=file:/frame4.raw dst=spi:2 size=153600 \
  wait=false

# Result: Hardware sends ALL 4 frames automatically!
# CPU usage: <1% (nur setup!)
```

**Advanced: Encryption Pipeline**
```bash
# File → AES → SHA → Network
dma_chain \
  src=file:/data.bin dst=aes:encrypt size=8192 \
  src=aes:output dst=sha:compute size=8192 \
  src=sha:output dst=network:0 size=32
```

---

### Command 3: `dma_stream`

**Purpose:** Continuous Streaming (with Loop!)

**Syntax:**
```bash
dma_stream src=<source> dst=<dest> [loop=<true|false>] [stop=<handle>]
```

**Example:**
```bash
# Audio Playback (Looping)
$handle = dma_stream src=file:/audio.wav dst=i2s:0 loop=true

# ... later ...
dma_stop handle=$handle
```

**Advanced: Real-Time Sensor Stream**
```bash
# ADC → File (Continuous)
dma_stream src=adc:0 dst=file:/sensor_log.bin loop=false
# Stops automatically when file size limit reached
```

---

### Command 4: `dma_status`

**Purpose:** Check DMA Transfer Status

**Syntax:**
```bash
dma_status handle=<handle>
```

**Returns:**
- `running` - Transfer in progress
- `complete` - Transfer finished
- `error` - Transfer failed

**Example:**
```bash
$handle = dma_copy src=file:/big.bin dst=spi:2 size=1048576 wait=false

while dma_status handle=$handle == running
  # Do other work
  delay_ms 10
endwhile

echo "Transfer complete!"
```

---

### Command 5: `dma_stop`

**Purpose:** Abort DMA Transfer

**Syntax:**
```bash
dma_stop handle=<handle>
```

---

### Command 6: `dma_stats`

**Purpose:** Get DMA Statistics

**Syntax:**
```bash
dma_stats
```

**Output:**
```
DMA Statistics:
├─ Channel 0 (TX): 1234 transfers, 45 MB total, 0 errors
├─ Channel 1 (TX): 567 transfers, 12 MB total, 0 errors
├─ Channel 2 (TX): IDLE
├─ Channel 0 (RX): 89 transfers, 2 MB total, 0 errors
├─ Channel 1 (RX): IDLE
└─ Channel 2 (RX): IDLE
```

---

## 🔧 ENHANCED EXISTING COMMANDS (DMA-Aware!)

### Modified: `spi_write` (now DMA-accelerated)

**Old:**
```bash
spi_write data 0x01 0x02 0x03  # Software, slow
```

**New (Auto-DMA if size > 64 bytes):**
```bash
spi_write data <bigdata>  # Automatically uses DMA!
```

**Explicit DMA Control:**
```bash
spi_write data <bigdata> dma=true
```

---

### Modified: `display_fill` (DMA-accelerated!)

**Syntax:**
```bash
display_fill x=<x> y=<y> w=<w> h=<h> color=<rgb565> [dma=<auto|true|false>]
```

**Implementation (Bare-Metal + GDMA):**
```c
void display_fill_dma(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Setup Window
    display_set_window(x, y, x+w-1, y+h-1);
    
    // Prepare DMA Buffer (filled with color)
    static uint16_t dma_buffer[1024] __attribute__((aligned(4)));
    for (int i = 0; i < 1024; i++) {
        dma_buffer[i] = color;
    }
    
    // Calculate total pixels
    uint32_t total_pixels = w * h;
    uint32_t buffers_needed = (total_pixels + 1023) / 1024;
    
    // Setup DMA Chain (Linked Descriptors!)
    gdma_descriptor_t* descriptors = alloc_dma_chain(buffers_needed);
    for (uint32_t i = 0; i < buffers_needed; i++) {
        descriptors[i].buffer = dma_buffer;
        descriptors[i].length = (i < buffers_needed - 1) ? 2048 : (total_pixels % 1024) * 2;
        descriptors[i].next = (i < buffers_needed - 1) ? &descriptors[i+1] : NULL;
        descriptors[i].flags = (i == buffers_needed - 1) ? GDMA_DESC_EOF : 0;
    }
    
    // Start DMA (Non-Blocking!)
    gdma_chain(spi_dma_channel, descriptors);
    gdma_start(spi_dma_channel);
    
    // CPU is FREE now! DMA sends everything!
}

// Performance:
// 320x240 fill (76800 pixels):
//   Software: ~50ms @ 80% CPU
//   DMA: ~5ms @ <1% CPU
// → 10x faster + 80x less CPU!
```

---

## 🏗️ INTERNAL ARCHITECTURE

### Layer 1: Hardware Abstraction (`gdma_hal.c`)

```c
// Direct Register Access (Bare-Metal!)

typedef struct {
    volatile uint32_t conf0;      // Configuration
    volatile uint32_t int_raw;    // Interrupt Raw
    volatile uint32_t int_st;     // Interrupt Status
    volatile uint32_t int_ena;    // Interrupt Enable
    volatile uint32_t int_clr;    // Interrupt Clear
    volatile uint32_t misc_conf;  // Misc Config
    volatile uint32_t in_link;    // RX Descriptor Address
    volatile uint32_t out_link;   // TX Descriptor Address
    // ... more registers
} gdma_channel_regs_t;

#define GDMA_CH0_TX ((gdma_channel_regs_t*)0x60080000)
#define GDMA_CH1_TX ((gdma_channel_regs_t*)0x60080080)
#define GDMA_CH2_TX ((gdma_channel_regs_t*)0x60080100)
// ... RX channels

// Low-Level Functions
static inline void gdma_ch_start(gdma_channel_regs_t* ch, gdma_descriptor_t* desc) {
    ch->out_link = (uint32_t)desc | BIT(31);  // Set address + START bit
}

static inline bool gdma_ch_is_done(gdma_channel_regs_t* ch) {
    return ch->int_raw & BIT(1);  // EOF interrupt
}

static inline void gdma_ch_clear_int(gdma_channel_regs_t* ch) {
    ch->int_clr = 0xFFFFFFFF;
}
```

---

### Layer 2: DMA Manager (`gdma_mgr.c`)

```c
// Channel Allocation & Management

typedef struct {
    bool allocated;
    uint32_t peripheral;  // GDMA_PERIPH_SPI2, etc.
    uint32_t priority;    // 0-2
    gdma_channel_regs_t* regs;
} gdma_channel_state_t;

static gdma_channel_state_t tx_channels[3];
static gdma_channel_state_t rx_channels[3];

gdma_channel_t* gdma_alloc(gdma_config_t* cfg) {
    // Find free channel
    for (int i = 0; i < 3; i++) {
        if (!tx_channels[i].allocated) {
            tx_channels[i].allocated = true;
            tx_channels[i].peripheral = cfg->peripheral;
            tx_channels[i].priority = cfg->priority;
            
            // Connect to peripheral
            gdma_connect_peripheral(i, cfg->peripheral);
            
            return &tx_channels[i];
        }
    }
    return NULL;  // No free channel!
}

void gdma_free(gdma_channel_t* ch) {
    ch->allocated = false;
    gdma_disconnect_peripheral(ch);
}
```

---

### Layer 3: Peripheral Adapters (`spi_dma.c`, `i2s_dma.c`, etc.)

```c
// SPI2 DMA Adapter

static gdma_channel_t* spi2_dma_ch = NULL;

esp_err_t spi_dma_init(void) {
    gdma_config_t cfg = {
        .channel = 0,  // Will be auto-assigned
        .peripheral = GDMA_PERIPH_SPI2,
        .priority = 2,  // High priority
        .round_robin = false
    };
    
    spi2_dma_ch = gdma_alloc(&cfg);
    if (!spi2_dma_ch) return ESP_ERR_NO_MEM;
    
    // Configure SPI2 for DMA
    SPI2.dma_conf.out_rst = 1;
    SPI2.dma_conf.out_rst = 0;
    SPI2.dma_conf.outdscr_burst_en = 1;  // Burst mode!
    
    return ESP_OK;
}

esp_err_t spi_dma_write(uint8_t* data, size_t len) {
    // Allocate descriptor
    gdma_descriptor_t* desc = heap_caps_malloc(sizeof(gdma_descriptor_t), MALLOC_CAP_DMA);
    desc->buffer = data;
    desc->length = len;
    desc->next = NULL;
    desc->flags = GDMA_DESC_EOF | GDMA_DESC_OWNER_DMA;
    
    // Start DMA
    gdma_ch_start(spi2_dma_ch->regs, desc);
    
    // Free descriptor when done (in interrupt or polling)
    // ...
    
    return ESP_OK;
}

esp_err_t spi_dma_write_chain(gdma_descriptor_t* desc_list) {
    // Just start the chain!
    gdma_ch_start(spi2_dma_ch->regs, desc_list);
    return ESP_OK;
}
```

---

### Layer 4: Command Interface (`cmd_dma.c`)

```c
// Command: dma_copy

int cmd_dma_copy(int argc, char** argv) {
    // Parse arguments
    char* src = get_arg(argv, "src");
    char* dst = get_arg(argv, "dst");
    size_t size = get_arg_int(argv, "size");
    bool wait = get_arg_bool(argv, "wait", false);
    
    // Validate
    if (!src || !dst || size == 0) {
        printf("Error: Missing arguments\n");
        return -1;
    }
    
    // Parse source
    void* src_ptr = NULL;
    if (strncmp(src, "file:", 5) == 0) {
        // Memory-mapped file!
        corefs_mmap_t* mmap = corefs_mmap(src + 5);
        if (!mmap) return -1;
        src_ptr = (void*)mmap->data;
    } else if (strncmp(src, "mem:", 4) == 0) {
        src_ptr = (void*)strtoul(src + 4, NULL, 16);
    } else {
        printf("Error: Invalid source\n");
        return -1;
    }
    
    // Parse destination (similar logic)
    // ...
    
    // Setup DMA
    gdma_descriptor_t desc = {
        .buffer = src_ptr,
        .length = size,
        .next = NULL,
        .flags = GDMA_DESC_EOF | GDMA_DESC_OWNER_DMA
    };
    
    // Start transfer (peripheral-specific)
    if (strncmp(dst, "spi:", 4) == 0) {
        spi_dma_write_chain(&desc);
    } else if (strncmp(dst, "i2s:", 4) == 0) {
        i2s_dma_write_chain(&desc);
    } // ... more peripherals
    
    // Wait if requested
    if (wait) {
        while (!gdma_ch_is_done(spi2_dma_ch->regs)) {
            vTaskDelay(1);
        }
        printf("DMA transfer complete\n");
    } else {
        printf("DMA transfer started (async)\n");
    }
    
    return 0;  // Or return handle for async
}
```

---

## 🧪 EDGE CASES & SOLUTIONS

### Edge Case 1: Buffer Not DMA-Compatible

**Problem:** User passes stack-allocated buffer
```c
uint8_t buffer[1024];  // On stack - NOT DMA-safe!
dma_copy src=mem:buffer dst=spi:2 size=1024  // CRASH!
```

**Solution:** Validate + Auto-Copy
```c
bool is_dma_capable(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    // Check if in SRAM range and 4-byte aligned
    return (addr >= 0x3FC00000 && addr < 0x3FD00000) && ((addr & 3) == 0);
}

if (!is_dma_capable(src_ptr)) {
    // Auto-allocate DMA buffer & copy
    void* dma_buf = heap_caps_malloc(size, MALLOC_CAP_DMA);
    memcpy(dma_buf, src_ptr, size);
    src_ptr = dma_buf;
    // Set flag to free later
}
```

---

### Edge Case 2: Cache Coherency

**Problem:** CPU writes to buffer → DMA reads stale data (still in cache!)

**Solution:** Explicit Cache Flush
```c
// Before DMA TX
Cache_WriteBack_Addr((uint32_t)buffer, size);

// After DMA RX
Cache_Invalidate_Addr((uint32_t)buffer, size);
```

**Automatic in DMA Commands:**
```c
void dma_prepare_buffer(void* buf, size_t size, bool is_tx) {
    if (is_tx) {
        Cache_WriteBack_Addr((uint32_t)buf, size);
    } else {
        Cache_Invalidate_Addr((uint32_t)buf, size);
    }
}
```

---

### Edge Case 3: Concurrent DMA Requests

**Problem:** User starts 2 DMA transfers on same channel

**Solution:** Channel Busy Check
```c
esp_err_t gdma_start(gdma_channel_t* ch) {
    if (ch->regs->out_link & BIT(31)) {
        // Channel busy!
        return ESP_ERR_INVALID_STATE;
    }
    // ... start
}
```

**Better: Queue System**
```c
typedef struct {
    gdma_descriptor_t* desc;
    void (*callback)(void);
} dma_request_t;

static queue_t* dma_queue;

esp_err_t gdma_queue_request(dma_request_t* req) {
    queue_push(dma_queue, req);
    if (!gdma_is_busy()) {
        gdma_process_next();
    }
    return ESP_OK;
}
```

---

### Edge Case 4: Power Management

**Problem:** DMA active → System can't enter Deep-Sleep

**Solution:** DMA Lock
```c
static esp_pm_lock_handle_t dma_pm_lock;

void gdma_start(gdma_channel_t* ch) {
    // Acquire power lock
    esp_pm_lock_acquire(dma_pm_lock);
    
    // ... start DMA
}

void gdma_done_isr(void) {
    // Release power lock
    esp_pm_lock_release(dma_pm_lock);
}
```

---

## 📊 PERFORMANCE VALIDATION

### Test 1: Display Refresh (320x240 RGB565)

**Setup:**
```bash
# Prepare framebuffer
file=/tmp/frame.raw size=153600

# Benchmark: Software vs DMA
time display_fill x=0 y=0 w=320 h=240 color=0xF800 dma=false
time display_fill x=0 y=0 w=320 h=240 color=0xF800 dma=true
```

**Expected Results:**
```
Software: 50ms @ 80% CPU
DMA: 5ms @ <1% CPU
→ 10x faster, 80x less CPU!
```

---

### Test 2: Audio Streaming (44.1 kHz, 16-bit Stereo)

**Setup:**
```bash
# Audio file: 10 seconds @ 176 KB/s = 1.76 MB
file=/audio/test.wav size=1760000

# Benchmark: Software vs DMA
time audio_play file=/audio/test.wav mode=software
time audio_play file=/audio/test.wav mode=dma  # or just dma_stream!
```

**Expected Results:**
```
Software: 60% CPU (continuous)
DMA: 5% CPU (nur setup!)
→ 12x less CPU load!
```

---

### Test 3: File Encryption (AES-256, 1 MB)

**Setup:**
```bash
# Test file
file=/data.bin size=1048576

# Benchmark: Software vs DMA
time aes_encrypt file=/data.bin key=... mode=software
time aes_encrypt file=/data.bin key=... mode=dma  # uses dma_copy internally
```

**Expected Results:**
```
Software: 100ms @ 100% CPU (10 MB/s)
DMA: 13ms @ 0% CPU (80 MB/s)
→ 8x faster, 100% less CPU!
```

---

## 🔄 INTEGRATION WITH STAPELDATEIEN

### Example 1: Animation Loop

```bash
# /stapel/animation.csp
# 60 FPS animation using DMA

for $frame in 0..59
  # Calculate frame index
  $file = "/anim/frame" + $frame + ".raw"
  
  # DMA transfer (non-blocking!)
  dma_copy src=file:$file dst=spi:2 size=153600 wait=false
  
  # Wait for next frame (16ms @ 60 FPS)
  # DMA completes in ~5ms, so we have 11ms free!
  delay_ms 16
endfor

# CPU usage: <10% (mostly loop overhead!)
# vs Software: 80%+ CPU!
```

---

### Example 2: Parallel Tasks

```bash
# /stapel/multitask.csp
# Display + Audio + Sensor Logging (parallel via DMA!)

# Start Audio (DMA streaming)
$audio_handle = dma_stream src=file:/music.wav dst=i2s:0 loop=true

# Start Sensor Logging (DMA continuous)
$sensor_handle = dma_stream src=adc:0 dst=file:/log.bin loop=false

# Display Animation Loop (DMA)
while true
  for $frame in 0..29
    $file = "/anim/frame" + $frame + ".raw"
    dma_copy src=file:$file dst=spi:2 size=153600 wait=false
    delay_ms 33  # 30 FPS
  endfor
endwhile

# Result:
# - Audio plays in background (5% CPU)
# - Sensor logs continuously (<1% CPU)
# - Display animates smoothly (5% CPU)
# TOTAL: ~10% CPU (vs 150%+ impossible with software!)
```

---

## ✅ COMPLETION CRITERIA

### Phase 1 Complete When:

```
□ GDMA HAL implemented (direct register access)
□ Channel Manager implemented (allocation/free)
□ SPI2-DMA adapter implemented
□ I2S-DMA adapter implemented
□ dma_copy command working
□ dma_chain command working
□ dma_stream command working
□ dma_status/stop commands working
□ Cache coherency handled
□ Buffer validation implemented
□ Power management integrated
□ Performance tests pass (10x speedup)
□ Edge cases tested & handled
□ Documentation complete
```

---

## 🎯 NEXT ITERATION PROMPT (Self-Generated)

**Question:** "ETM Integration: Wie verbinde ich Events mit DMA?"

**Answer Preview:**
```
ETM kann:
├─ GPIO Edge → DMA Start (Button drücken = Display update!)
├─ Timer Alarm → DMA Copy (Periodic sensor logging!)
├─ DMA Done → GPIO Toggle (LED blinkt wenn Transfer fertig!)
└─ ADC Threshold → DMA Start (Sensor überschreitet Limit = Log!)

Alle OHNE CPU Interrupt!
```

**Continue to Iteration 3...**

---

**END OF ITERATION 2**

*[Autonomous loop continues...]*
