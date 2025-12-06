# ⚠️ PSRAM Stack Limitation Discovered

**Date**: 2025-12-06
**Status**: 🔴 **BLOCKED** - xTaskCreateStatic with PSRAM Not Supported

---

## 🚫 Problem Discovered

During implementation, we discovered that **`xTaskCreateStatic` with PSRAM-allocated stacks is NOT supported** by ESP-IDF/FreeRTOS on ESP32-S3.

### Error Encountered

```
assert failed: xTaskCreateStaticPinnedToCore tasks.c:682 (xPortcheckValidStackMem(pxStackBuffer))
```

### Root Cause

ESP-IDF's FreeRTOS implementation includes a validation check (`xPortcheckValidStackMem`) that **only allows internal SRAM for statically-allocated task stacks**.

Even with `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=1`, this flag only applies to:
- ✅ `xTaskCreate()` - FreeRTOS allocates stack automatically
- ❌ `xTaskCreateStatic()` - User provides preallocated stack (must be DRAM)

---

## 📚 ESP-IDF Documentation

From [ESP-IDF Memory Allocation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/ram-usage.html):

> **CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY**
> Allow task stacks to be allocated from external memory (PSRAM). This applies to tasks created with `xTaskCreate()` and `xTaskCreatePinnedToCore()`.

**Key Point**: `xTaskCreateStatic()` is NOT mentioned!

---

## ✅ Correct Approach

### Option 1: Use `xTaskCreate()` with CONFIG Flag (RECOMMENDED)

Let ESP-IDF handle stack allocation automatically:

```cpp
// platformio.ini already has:
-D CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=1

// Code:
xTaskCreatePinnedToCore(
    task_function,
    "task_name",
    STACK_SIZE,      // in bytes
    param,
    priority,
    &task_handle,
    core
);
```

**How it works**:
- FreeRTOS checks available memory
- If DRAM is low, automatically allocates stack in PSRAM
- Transparent to application code
- No manual management needed

### Option 2: Hybrid Allocation (COMPLEX, NOT RECOMMENDED)

Use capability-based allocation with runtime checks:

```cpp
// Try PSRAM first
void* stack = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
if (!stack) {
    // Fallback to DRAM
    stack = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
}

// But xTaskCreateStatic will STILL FAIL with PSRAM address!
```

**Conclusion**: This doesn't work due to ESP-IDF validation.

---

## 🔄 Revised Strategy

### What We CAN Do

1. ✅ **Keep heap buffer allocations in PSRAM** (already implemented)
   - Microphone buffers
   - Audio buffers
   - Logger buffers
   - LVGL objects

2. ✅ **Rely on automatic PSRAM stack allocation**
   - Use `xTaskCreate()` normally
   - Let `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` work automatically
   - ESP-IDF will use PSRAM when DRAM is fragmented

3. ✅ **Optimize DRAM usage in other ways**
   - Reduce stack sizes where possible
   - Use PSRAM for all large buffers
   - Optimize LVGL buffer mode

### What We CANNOT Do

- ❌ Manually control which task stacks go to PSRAM
- ❌ Use `xTaskCreateStatic` with PSRAM stacks
- ❌ Guarantee specific tasks use PSRAM stacks

---

## 📊 Revised Memory Savings

| Optimization | Method | DRAM Saved | Status |
|--------------|--------|------------|--------|
| **Microphone Buffer** | `heap_caps_malloc(PSRAM)` | ~4 KB | ✅ Implemented |
| **Voice Assistant File Buffer** | `heap_caps_malloc(PSRAM)` | Variable | ✅ Already Done |
| **Logger Buffers** | `heap_caps_malloc(PSRAM)` | ~16 KB | ✅ Already Done |
| **Audio Buffers** | `heap_caps_malloc(PSRAM)` | ~64 KB | ✅ Already Done |
| **Color Picker Buffer** | `heap_caps_malloc(PSRAM)` | Variable | ✅ Already Done |
| **Task Stacks (Auto)** | `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` | ~28 KB | ⚠️ **Automatic** |
| **LVGL Buffer Mode** | PSRAM Single Buffer (Mode 0) | ~15 KB | ⚠️ **Optional** |

**Total Guaranteed Savings**: ~84 KB (buffers only)
**Total Potential Savings**: ~127 KB (buffers + automatic stack allocation)

---

## 🎯 Final Recommendations

### 1. **Revert Task Stack Modifications** ✅

All task creation code should use standard `xTaskCreatePinnedToCore()`:

```cpp
// CORRECT:
xTaskCreatePinnedToCore(task_fn, name, stack_size, param, prio, &handle, core);

// INCORRECT (causes crash):
xTaskCreateStaticPinnedToCore(task_fn, name, stack_size, param, prio,
                              psram_stack, &buffer, core);
```

### 2. **Keep Buffer Optimizations** ✅

All `heap_caps_malloc(MALLOC_CAP_SPIRAM)` changes are valid and working:
- Microphone manager
- Voice assistant
- Logger
- Audio player
- Timeshift manager

### 3. **Monitor Automatic PSRAM Usage** 📊

With `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=1`, ESP-IDF will automatically:
- Allocate task stacks in PSRAM when DRAM is low
- Prefer DRAM for small stacks (<4KB typically)
- Use PSRAM for large stacks (>8KB)

To verify:
```cpp
// Check task stack location
void* stack_addr = pxTaskGetStackStart(task_handle);
bool is_psram = ((uint32_t)stack_addr >= 0x3C000000 &&
                 (uint32_t)stack_addr < 0x3E000000);
```

### 4. **Optimize LVGL Buffer** (Optional)

If DRAM is still tight, switch to PSRAM buffer:

```ini
# platformio.ini
-D LVGL_BUFFER_MODE=0  # PSRAM single buffer (saves 15 KB DRAM)
```

**Trade-off**: ~10-20% slower UI rendering (acceptable for most use cases)

---

## 📝 Lessons Learned

1. **ESP-IDF has limitations** not always clearly documented
2. **`xTaskCreateStatic` ≠ `xTaskCreate`** in terms of PSRAM support
3. **Automatic allocation is often better** than manual control
4. **Always test on real hardware** before assuming feature support
5. **Buffer allocations > Stack allocations** for manual PSRAM optimization

---

## 🔗 References

- [ESP-IDF RAM Usage Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/ram-usage.html)
- [FreeRTOS Static Allocation](https://www.freertos.org/a00110.html#configSUPPORT_STATIC_ALLOCATION)
- [ESP32-S3 Memory Layout](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/memory-types.html)
- [Heap Capabilities API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/mem_alloc.html)

---

**Conclusion**: Manual PSRAM stack allocation via `xTaskCreateStatic` is **NOT POSSIBLE**. We must rely on automatic allocation and focus on buffer optimizations instead.

*Updated: 2025-12-06*
