# CoreFS B-Tree Fix - COMPLETE IMPLEMENTATION
## Autonomous Iteration 5 | Critical Bug Fix

---

## 🐛 THE BUG (Precise Analysis)

**Current Behavior (from README):**
```
Test 1: Create '/test.txt' → ✅ OK
Test 2: Open '/test.txt'   → ❌ File not found
Test 3: Read '/test.txt'   → ❌ File not found
Test 4: Append '/test.txt' → ❌ File not found
```

**Root Cause Analysis:**
```c
// corefs_btree.c - Current Implementation (BROKEN!)

esp_err_t corefs_btree_init(corefs_ctx_t* ctx) {
    // ❌ BUG: Alloc in RAM, never written to Flash!
    corefs_btree_node_t* root = calloc(1, sizeof(corefs_btree_node_t));
    root->magic = COREFS_BLOCK_MAGIC;
    root->type = 1;  // Leaf
    root->count = 0;
    
    // ❌ MISSING: corefs_block_write(ctx, ctx->sb->root_block, root);
    free(root);  // ❌ Lost forever!
    return ESP_OK;
}

esp_err_t corefs_btree_insert(corefs_ctx_t* ctx, const char* name, uint32_t inode) {
    // ❌ BUG: Alloc in RAM, modify, never write back!
    corefs_btree_node_t* root = calloc(1, sizeof(corefs_btree_node_t));
    
    // Read from flash... NO WAIT, IT DOESN'T!
    // ❌ It just creates empty node!
    
    // Insert entry
    root->entries[root->count].inode = inode;
    root->entries[root->count].name_hash = hash_name(name);
    strncpy(root->entries[root->count].name, name, 64);
    root->count++;
    
    // ❌ MISSING: corefs_block_write(ctx, ctx->sb->root_block, root);
    free(root);  // ❌ Lost forever!
    return ESP_OK;
}
```

**Why it "works" in single session:**
```
Session 1:
  1. Format → Create empty root (in RAM, lost)
  2. Mount → Create empty root (in RAM)
  3. Create file → Insert entry (in RAM root)
  4. Open file → Find entry (in SAME RAM root) → ✅ OK!

After Reboot:
  1. Mount → Create empty root (in RAM, DIFFERENT instance!)
  2. Open file → Find entry (NOT in this RAM root) → ❌ NOT FOUND!
```

**The Fix (Overview):**
```
1. Format: Write Root Node to Flash (Block 1)
2. Mount: Read Root Node from Flash (Block 1)
3. Insert: Read Node → Modify → Write back to Flash
4. Find: Read Node from Flash (not RAM!)
```

---

## 🔧 THE FIX (Step-by-Step Implementation)

### STEP 1: Modify `corefs_btree_init()` (Format-time)

**File:** `components/corefs/src/corefs_btree.c`

**OLD CODE:**
```c
esp_err_t corefs_btree_init(corefs_ctx_t* ctx) {
    corefs_btree_node_t* root = calloc(1, sizeof(corefs_btree_node_t));
    if (!root) return ESP_ERR_NO_MEM;
    
    root->magic = COREFS_BLOCK_MAGIC;
    root->type = 1;  // Leaf
    root->count = 0;
    root->parent = 0;
    
    free(root);  // ❌ BUG: Lost forever!
    return ESP_OK;
}
```

**NEW CODE (FIXED):**
```c
esp_err_t corefs_btree_init(corefs_ctx_t* ctx) {
    // Allocate DMA-capable buffer
    corefs_btree_node_t* root = heap_caps_calloc(1, COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate root node");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize Root Node
    root->magic = COREFS_BLOCK_MAGIC;
    root->type = 1;  // Leaf
    root->count = 0;
    root->parent = 0;
    
    // ✅ FIX: Write Root Node to Flash!
    esp_err_t ret = corefs_block_write(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write root node: %s", esp_err_to_name(ret));
        free(root);
        return ret;
    }
    
    ESP_LOGI(TAG, "B-Tree root initialized at block %u", ctx->sb->root_block);
    
    free(root);
    return ESP_OK;
}
```

**Key Changes:**
1. `heap_caps_calloc()` instead of `calloc()` → DMA-safe buffer
2. `COREFS_BLOCK_SIZE` instead of `sizeof()` → Full block for alignment
3. **`corefs_block_write()`** → Write to Flash!
4. Logging for debugging

---

### STEP 2: Add `corefs_btree_load()` (Mount-time)

**File:** `components/corefs/src/corefs_btree.c`

**NEW FUNCTION:**
```c
/**
 * Load B-Tree Root Node from Flash
 * Called during mount
 */
esp_err_t corefs_btree_load(corefs_ctx_t* ctx) {
    // Allocate temporary buffer
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // ✅ FIX: Read Root Node from Flash!
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read root node: %s", esp_err_to_name(ret));
        free(root);
        return ret;
    }
    
    // Verify Magic
    if (root->magic != COREFS_BLOCK_MAGIC) {
        ESP_LOGE(TAG, "Invalid root node magic: 0x%08X (expected 0x%08X)",
                 root->magic, COREFS_BLOCK_MAGIC);
        free(root);
        return ESP_ERR_INVALID_CRC;
    }
    
    ESP_LOGI(TAG, "B-Tree root loaded: type=%u, count=%u", root->type, root->count);
    
    // TODO: If you want to keep root in RAM for faster access:
    // ctx->btree_root = root;  // Store pointer
    // Otherwise just verify and free:
    free(root);
    
    return ESP_OK;
}
```

**Add to Header:**
```c
// components/corefs/include/corefs.h

esp_err_t corefs_btree_init(corefs_ctx_t* ctx);
esp_err_t corefs_btree_load(corefs_ctx_t* ctx);  // ← NEW!
```

---

### STEP 3: Call `corefs_btree_load()` in Mount

**File:** `components/corefs/src/corefs_core.c`

**FIND:**
```c
esp_err_t corefs_mount(const esp_partition_t* partition) {
    // ... existing code ...
    
    ret = corefs_block_init(&g_ctx);
    if (ret != ESP_OK) goto cleanup;
    
    // ❌ MISSING: Load B-Tree!
    
    g_ctx.mounted = true;
    // ...
}
```

**CHANGE TO:**
```c
esp_err_t corefs_mount(const esp_partition_t* partition) {
    // ... existing code ...
    
    ret = corefs_block_init(&g_ctx);
    if (ret != ESP_OK) goto cleanup;
    
    // ✅ FIX: Load B-Tree from Flash!
    ret = corefs_btree_load(&g_ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load B-Tree, filesystem may be corrupted");
        goto cleanup;
    }
    
    g_ctx.mounted = true;
    // ...
}
```

---

### STEP 4: Modify `corefs_btree_insert()` (Persistent Writes!)

**File:** `components/corefs/src/corefs_btree.c`

**OLD CODE:**
```c
esp_err_t corefs_btree_insert(corefs_ctx_t* ctx, const char* name, uint32_t inode) {
    corefs_btree_node_t* root = calloc(1, sizeof(corefs_btree_node_t));
    if (!root) return ESP_ERR_NO_MEM;
    
    // ❌ BUG: Doesn't read from Flash!
    // ... insert logic ...
    
    free(root);  // ❌ Lost!
    return ESP_OK;
}
```

**NEW CODE (FIXED):**
```c
esp_err_t corefs_btree_insert(corefs_ctx_t* ctx, const char* name, uint32_t inode) {
    // Allocate DMA-safe buffer
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate node");
        return ESP_ERR_NO_MEM;
    }
    
    // ✅ FIX: Read Root Node from Flash!
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read root node");
        free(root);
        return ret;
    }
    
    // Verify Magic
    if (root->magic != COREFS_BLOCK_MAGIC) {
        ESP_LOGE(TAG, "Corrupted root node");
        free(root);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check if node is full
    if (root->count >= COREFS_BTREE_ORDER - 1) {
        ESP_LOGE(TAG, "B-Tree node full (TODO: split)");
        free(root);
        return ESP_ERR_NO_MEM;
    }
    
    // Insert Entry
    int idx = root->count;
    root->entries[idx].inode = inode;
    root->entries[idx].name_hash = hash_name(name);
    strncpy(root->entries[idx].name, name, sizeof(root->entries[idx].name) - 1);
    root->count++;
    
    // ✅ FIX: Write Modified Node back to Flash!
    ret = corefs_block_write(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write root node");
        free(root);
        return ret;
    }
    
    ESP_LOGD(TAG, "Inserted '%s' (inode %u) into B-Tree, count now %u",
             name, inode, root->count);
    
    free(root);
    return ESP_OK;
}
```

**Key Changes:**
1. **Read Node from Flash** before modify!
2. **Write Node back to Flash** after modify!
3. Verify Magic for corruption detection
4. Check for full node (TODO: node split)

---

### STEP 5: Modify `corefs_btree_find()` (Read from Flash!)

**File:** `components/corefs/src/corefs_btree.c`

**OLD CODE:**
```c
int32_t corefs_btree_find(corefs_ctx_t* ctx, const char* path) {
    // ... parse path ...
    
    corefs_btree_node_t* node = calloc(1, sizeof(corefs_btree_node_t));
    if (!node) return -1;
    
    // ❌ BUG: Doesn't read from Flash!
    // node is empty!
    
    // ... search logic (finds nothing!) ...
    
    free(node);
    return -1;  // ❌ Always not found!
}
```

**NEW CODE (FIXED):**
```c
int32_t corefs_btree_find(corefs_ctx_t* ctx, const char* path) {
    if (!path || path[0] != '/') return -1;
    
    // Parse path
    char path_copy[COREFS_MAX_PATH];
    strncpy(path_copy, path + 1, sizeof(path_copy) - 1);
    
    // For now, only root directory (no subdirs)
    // TODO: Navigate subdirectories
    
    // Allocate buffer
    corefs_btree_node_t* node = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!node) return -1;
    
    // ✅ FIX: Read Root Node from Flash!
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, node);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read root node");
        free(node);
        return -1;
    }
    
    // Verify Magic
    if (node->magic != COREFS_BLOCK_MAGIC) {
        ESP_LOGE(TAG, "Corrupted root node");
        free(node);
        return -1;
    }
    
    // Search for file
    const char* filename = path_copy;  // Simplified (no subdirs)
    uint32_t hash = hash_name(filename);
    
    for (int i = 0; i < node->count; i++) {
        if (node->entries[i].name_hash == hash &&
            strcmp(node->entries[i].name, filename) == 0) {
            
            uint32_t inode_num = node->entries[i].inode;
            
            ESP_LOGD(TAG, "Found '%s' → inode %u", filename, inode_num);
            
            free(node);
            return (int32_t)inode_num;
        }
    }
    
    ESP_LOGD(TAG, "File '%s' not found in B-Tree", filename);
    
    free(node);
    return -1;  // Not found
}
```

---

### STEP 6: Modify `corefs_btree_delete()` (Persistent Deletes!)

**File:** `components/corefs/src/corefs_btree.c`

**NEW CODE (FIXED):**
```c
esp_err_t corefs_btree_delete(corefs_ctx_t* ctx, const char* name) {
    // Allocate buffer
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) return ESP_ERR_NO_MEM;
    
    // ✅ FIX: Read Root Node from Flash!
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        free(root);
        return ret;
    }
    
    // Search and Delete
    uint32_t hash = hash_name(name);
    
    for (int i = 0; i < root->count; i++) {
        if (root->entries[i].name_hash == hash &&
            strcmp(root->entries[i].name, name) == 0) {
            
            // Shift remaining entries
            memmove(&root->entries[i], &root->entries[i + 1],
                    (root->count - i - 1) * sizeof(root->entries[0]));
            root->count--;
            
            // ✅ FIX: Write Modified Node back to Flash!
            ret = corefs_block_write(ctx, ctx->sb->root_block, root);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write root node");
            }
            
            free(root);
            return ret;
        }
    }
    
    free(root);
    return ESP_ERR_NOT_FOUND;
}
```

---

## 🧪 TESTING STRATEGY

### Test 1: Basic Persistence (CRITICAL!)

```c
// test/test_btree_persistence.c

void test_btree_persistence(void) {
    ESP_LOGI(TAG, "=== TEST 1: B-Tree Persistence ===");
    
    // Format
    ESP_LOGI(TAG, "Step 1: Format");
    ESP_ERROR_CHECK(corefs_format());
    
    // Mount
    ESP_LOGI(TAG, "Step 2: Mount");
    ESP_ERROR_CHECK(corefs_mount());
    
    // Create File
    ESP_LOGI(TAG, "Step 3: Create file");
    corefs_file_t* file = corefs_open("/test.txt", COREFS_O_CREAT | COREFS_O_WRONLY);
    assert(file != NULL);
    
    // Write Data
    const char* data = "Hello CoreFS!";
    int written = corefs_write(file, data, strlen(data));
    assert(written == strlen(data));
    
    // Close
    corefs_close(file);
    
    // Unmount
    ESP_LOGI(TAG, "Step 4: Unmount");
    corefs_unmount();
    
    // ✅ CRITICAL: Remount (simulates reboot!)
    ESP_LOGI(TAG, "Step 5: Remount (simulates reboot)");
    ESP_ERROR_CHECK(corefs_mount());
    
    // ✅ TEST: Can we find the file now?
    ESP_LOGI(TAG, "Step 6: Open file after remount");
    file = corefs_open("/test.txt", COREFS_O_RDONLY);
    
    if (file == NULL) {
        ESP_LOGE(TAG, "❌ TEST FAILED: File not found after remount!");
        ESP_LOGE(TAG, "B-Tree is NOT persistent!");
        assert(false);
    }
    
    // Read Data
    char buf[64];
    int read = corefs_read(file, buf, sizeof(buf));
    buf[read] = '\0';
    
    if (strcmp(buf, data) != 0) {
        ESP_LOGE(TAG, "❌ TEST FAILED: Data mismatch!");
        ESP_LOGE(TAG, "Expected: %s", data);
        ESP_LOGE(TAG, "Got: %s", buf);
        assert(false);
    }
    
    ESP_LOGI(TAG, "✅ TEST PASSED: B-Tree is persistent!");
    ESP_LOGI(TAG, "Data: %s", buf);
    
    corefs_close(file);
    corefs_unmount();
}
```

---

### Test 2: Multiple Files Persistence

```c
void test_multiple_files_persistence(void) {
    ESP_LOGI(TAG, "=== TEST 2: Multiple Files ===");
    
    // Format & Mount
    ESP_ERROR_CHECK(corefs_format());
    ESP_ERROR_CHECK(corefs_mount());
    
    // Create 5 files
    const char* files[] = {
        "/file1.txt",
        "/file2.txt",
        "/file3.txt",
        "/file4.txt",
        "/file5.txt"
    };
    
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Creating %s", files[i]);
        corefs_file_t* f = corefs_open(files[i], COREFS_O_CREAT | COREFS_O_WRONLY);
        assert(f != NULL);
        
        char data[64];
        snprintf(data, sizeof(data), "Data for file %d", i + 1);
        corefs_write(f, data, strlen(data));
        corefs_close(f);
    }
    
    // Unmount & Remount
    corefs_unmount();
    ESP_LOGI(TAG, "Remounting...");
    ESP_ERROR_CHECK(corefs_mount());
    
    // Verify all files
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Verifying %s", files[i]);
        corefs_file_t* f = corefs_open(files[i], COREFS_O_RDONLY);
        
        if (f == NULL) {
            ESP_LOGE(TAG, "❌ File %s not found!", files[i]);
            assert(false);
        }
        
        char buf[64];
        int read = corefs_read(f, buf, sizeof(buf));
        buf[read] = '\0';
        
        ESP_LOGI(TAG, "  Data: %s", buf);
        corefs_close(f);
    }
    
    ESP_LOGI(TAG, "✅ All files persistent!");
    corefs_unmount();
}
```

---

### Test 3: File Delete Persistence

```c
void test_delete_persistence(void) {
    ESP_LOGI(TAG, "=== TEST 3: Delete Persistence ===");
    
    // Format & Mount
    ESP_ERROR_CHECK(corefs_format());
    ESP_ERROR_CHECK(corefs_mount());
    
    // Create file
    corefs_file_t* f = corefs_open("/temp.txt", COREFS_O_CREAT | COREFS_O_WRONLY);
    assert(f != NULL);
    corefs_write(f, "temp", 4);
    corefs_close(f);
    
    // Verify exists
    assert(corefs_exists("/temp.txt") == true);
    
    // Delete
    ESP_LOGI(TAG, "Deleting /temp.txt");
    ESP_ERROR_CHECK(corefs_unlink("/temp.txt"));
    
    // Verify deleted
    assert(corefs_exists("/temp.txt") == false);
    
    // Unmount & Remount
    corefs_unmount();
    ESP_LOGI(TAG, "Remounting...");
    ESP_ERROR_CHECK(corefs_mount());
    
    // ✅ TEST: File should STILL be gone!
    if (corefs_exists("/temp.txt")) {
        ESP_LOGE(TAG, "❌ Delete NOT persistent!");
        assert(false);
    }
    
    ESP_LOGI(TAG, "✅ Delete is persistent!");
    corefs_unmount();
}
```

---

## 📊 VALIDATION CHECKLIST

### Before Fix:
```
□ Test 1: Create file → ✅ OK
□ Test 2: Remount & Open → ❌ FAIL (File not found)
□ Test 3: Read data → ❌ FAIL (Can't open)
□ Test 4: Multiple files → ❌ FAIL (All lost after remount)
□ Test 5: Delete persistence → ❌ FAIL (File reappears!)
```

### After Fix (Expected):
```
□ Test 1: Create file → ✅ OK
□ Test 2: Remount & Open → ✅ OK (File found!)
□ Test 3: Read data → ✅ OK (Data intact!)
□ Test 4: Multiple files → ✅ OK (All persist!)
□ Test 5: Delete persistence → ✅ OK (Deleted stays deleted!)
```

---

## 🔍 DEBUGGING TIPS

### Enable Verbose Logging

```c
// In component config (menuconfig or sdkconfig)
CONFIG_LOG_DEFAULT_LEVEL=ESP_LOG_DEBUG

// Or in code:
esp_log_level_set("corefs_btree", ESP_LOG_DEBUG);
```

### Add Hexdump for Node Contents

```c
// In corefs_btree.c (after reading node)
ESP_LOG_BUFFER_HEX_LEVEL(TAG, root, 256, ESP_LOG_DEBUG);
```

### Verify Flash Contents Directly

```c
// Read raw block and inspect
uint8_t buf[2048];
const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
esp_partition_read(part, 0x0 + (1 * 2048), buf, 2048);  // Block 1 = Root

ESP_LOG_BUFFER_HEX(TAG, buf, 256);
```

---

## ⚠️ EDGE CASES TO TEST

### Edge Case 1: Power Loss During Write

**Scenario:** Power fails during `corefs_block_write()` in `btree_insert()`

**Expected:** Previous node state intact (Copy-on-Write!)

**Test:**
```c
// Simulate by killing power mid-write (hardware test!)
// Or software: abort() during write, check recovery on next boot
```

---

### Edge Case 2: Corrupted Node

**Scenario:** Flash bit-flip corrupts node magic

**Expected:** `btree_load()` detects and returns error

**Test:**
```c
void test_corrupted_node(void) {
    // Format & Create file
    // ...
    
    // Unmount
    corefs_unmount();
    
    // Corrupt Node (flip magic bytes)
    const esp_partition_t* part = /* ... */;
    uint8_t buf[2048];
    esp_partition_read(part, 0x0 + (1 * 2048), buf, 2048);
    buf[0] = 0xFF;  // Corrupt magic
    buf[1] = 0xFF;
    esp_partition_erase_range(part, 0x0 + (1 * 2048), 4096);
    esp_partition_write(part, 0x0 + (1 * 2048), buf, 2048);
    
    // Try to mount
    esp_err_t ret = corefs_mount();
    assert(ret == ESP_ERR_INVALID_CRC);  // Should fail!
    
    ESP_LOGI(TAG, "✅ Corruption detected!");
}
```

---

### Edge Case 3: Node Full (7 entries)

**Scenario:** Insert 8th file when node has max 7 entries (BTREE_ORDER=8)

**Expected:** Error (until node split is implemented!)

**Test:**
```c
void test_node_full(void) {
    // Create 7 files (max for order-8 tree)
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof(name), "/file%d.txt", i);
        corefs_file_t* f = corefs_open(name, COREFS_O_CREAT | COREFS_O_WRONLY);
        assert(f != NULL);
        corefs_close(f);
    }
    
    // Try to create 8th file
    corefs_file_t* f = corefs_open("/file8.txt", COREFS_O_CREAT | COREFS_O_WRONLY);
    assert(f == NULL);  // Should fail (node full)
    
    ESP_LOGI(TAG, "✅ Node full detected!");
}
```

---

## 📦 COMPLETE CODE FILES

### Updated `corefs_btree.c` (Full File)

```c
// components/corefs/src/corefs_btree.c
#include "corefs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "corefs_btree";

extern esp_err_t corefs_block_read(corefs_ctx_t* ctx, uint32_t block, void* buf);
extern esp_err_t corefs_block_write(corefs_ctx_t* ctx, uint32_t block, const void* buf);

// FNV-1a hash
static uint32_t hash_name(const char* name) {
    uint32_t hash = 2166136261u;
    while (*name) {
        hash ^= (uint8_t)*name++;
        hash *= 16777619u;
    }
    return hash;
}

/**
 * Initialize B-Tree Root Node (Format time)
 * ✅ FIXED: Now writes to Flash!
 */
esp_err_t corefs_btree_init(corefs_ctx_t* ctx) {
    corefs_btree_node_t* root = heap_caps_calloc(1, COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate root node");
        return ESP_ERR_NO_MEM;
    }
    
    root->magic = COREFS_BLOCK_MAGIC;
    root->type = 1;  // Leaf
    root->count = 0;
    root->parent = 0;
    
    esp_err_t ret = corefs_block_write(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write root node: %s", esp_err_to_name(ret));
        free(root);
        return ret;
    }
    
    ESP_LOGI(TAG, "B-Tree root initialized at block %u", ctx->sb->root_block);
    free(root);
    return ESP_OK;
}

/**
 * Load B-Tree Root Node from Flash (Mount time)
 * ✅ NEW: Reads from Flash and verifies!
 */
esp_err_t corefs_btree_load(corefs_ctx_t* ctx) {
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read root node: %s", esp_err_to_name(ret));
        free(root);
        return ret;
    }
    
    if (root->magic != COREFS_BLOCK_MAGIC) {
        ESP_LOGE(TAG, "Invalid root node magic: 0x%08X", root->magic);
        free(root);
        return ESP_ERR_INVALID_CRC;
    }
    
    ESP_LOGI(TAG, "B-Tree root loaded: type=%u, count=%u", root->type, root->count);
    free(root);
    return ESP_OK;
}

/**
 * Find Inode by Path
 * ✅ FIXED: Now reads from Flash!
 */
int32_t corefs_btree_find(corefs_ctx_t* ctx, const char* path) {
    if (!path || path[0] != '/') return -1;
    
    char path_copy[COREFS_MAX_PATH];
    strncpy(path_copy, path + 1, sizeof(path_copy) - 1);
    
    corefs_btree_node_t* node = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!node) return -1;
    
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, node);
    if (ret != ESP_OK) {
        free(node);
        return -1;
    }
    
    if (node->magic != COREFS_BLOCK_MAGIC) {
        free(node);
        return -1;
    }
    
    const char* filename = path_copy;
    uint32_t hash = hash_name(filename);
    
    for (int i = 0; i < node->count; i++) {
        if (node->entries[i].name_hash == hash &&
            strcmp(node->entries[i].name, filename) == 0) {
            uint32_t inode_num = node->entries[i].inode;
            free(node);
            return (int32_t)inode_num;
        }
    }
    
    free(node);
    return -1;
}

/**
 * Insert Entry into B-Tree
 * ✅ FIXED: Now reads, modifies, writes back!
 */
esp_err_t corefs_btree_insert(corefs_ctx_t* ctx, const char* name, uint32_t inode) {
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        free(root);
        return ret;
    }
    
    if (root->magic != COREFS_BLOCK_MAGIC) {
        free(root);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (root->count >= COREFS_BTREE_ORDER - 1) {
        ESP_LOGE(TAG, "B-Tree node full (TODO: split)");
        free(root);
        return ESP_ERR_NO_MEM;
    }
    
    int idx = root->count;
    root->entries[idx].inode = inode;
    root->entries[idx].name_hash = hash_name(name);
    strncpy(root->entries[idx].name, name, sizeof(root->entries[idx].name) - 1);
    root->count++;
    
    ret = corefs_block_write(ctx, ctx->sb->root_block, root);
    free(root);
    return ret;
}

/**
 * Delete Entry from B-Tree
 * ✅ FIXED: Now reads, modifies, writes back!
 */
esp_err_t corefs_btree_delete(corefs_ctx_t* ctx, const char* name) {
    corefs_btree_node_t* root = heap_caps_malloc(COREFS_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!root) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = corefs_block_read(ctx, ctx->sb->root_block, root);
    if (ret != ESP_OK) {
        free(root);
        return ret;
    }
    
    uint32_t hash = hash_name(name);
    
    for (int i = 0; i < root->count; i++) {
        if (root->entries[i].name_hash == hash &&
            strcmp(root->entries[i].name, name) == 0) {
            
            memmove(&root->entries[i], &root->entries[i + 1],
                    (root->count - i - 1) * sizeof(root->entries[0]));
            root->count--;
            
            ret = corefs_block_write(ctx, ctx->sb->root_block, root);
            free(root);
            return ret;
        }
    }
    
    free(root);
    return ESP_ERR_NOT_FOUND;
}
```

---

## ✅ COMPLETION CRITERIA

```
□ corefs_btree_init() writes root to Flash
□ corefs_btree_load() reads root from Flash
□ corefs_btree_insert() reads, modifies, writes
□ corefs_btree_find() reads from Flash
□ corefs_btree_delete() reads, modifies, writes
□ All functions use DMA-safe buffers
□ All functions verify node magic
□ Test 1 (Create → Reboot → Read) passes
□ Test 2 (Multiple files) passes
□ Test 3 (Delete persistence) passes
□ Edge cases handled
□ Logging added for debugging
□ README updated
```

---

## 🎯 NEXT ITERATION PROMPT

**Question:** "Wie designe ich den Stapeldatei Parser? Lexer, Parser, Executor - vollständige Architektur!"

**Continue to Iteration 6...**

---

**END OF ITERATION 5**

*[Autonomous loop continues...]*
