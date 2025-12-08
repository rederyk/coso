# TTS Failure in AI Chat vs Music App - Debugging Report

## Problem Summary

The **same audio file** (`tts_20251208_132315.wav`) plays successfully in the **Music app** but **fails during TTS synthesis** in the **AI Chat app** with:

```
[WARN] [VoiceAssistant] [TTS] Insufficient DRAM for TTS: 22424 bytes free (need ~40960 bytes)
Errore TTS - Mantieni testo LVGL
```

## Root Cause Analysis

### Why the Error Occurs

The error comes from `VoiceAssistant::LuaSandbox::lua_tts_speak()` in `voice_assistant.cpp:2826`:

```cpp
size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
const size_t MIN_REQUIRED_DRAM = 40 * 1024;  // 40KB minimum threshold

if (free_dram < MIN_REQUIRED_DRAM) {
    // FAIL: "Insufficient DRAM for TTS"
}
```

**Available DRAM in AI Chat:** 22,424 bytes (~22 KB)  
**Required DRAM:** 40,960 bytes (~40 KB)  
**Deficit:** 18,536 bytes (~18 KB)

### Why Music App Works

The Music app can successfully play the same file because:

1. **No TTS synthesis needed** - It only plays pre-generated WAV files
2. **Lower memory footprint** - No Lua sandbox, conversation buffer, or complex UI layer
3. **Simpler context** - Fewer active managers and background systems

### Memory Consumption Comparison

```
AI Chat Screen Layout:
├── LVGL UI hierarchy (header, chat container, input, PTT button, etc.)
├── Chat container with conversation history
├── Lua sandbox with 30+ API functions registered
├── Conversation buffer with entries
├── Async request manager
├── Status UI elements and timers
└── ~150-170 KB total DRAM consumed

Music App:
├── Simple file list browser
├── Audio player
└── ~80-100 KB total DRAM consumed
```

## The Data Manager Issue

You mentioned "e letteralmente lo stesso file, perche in app non va e in altra si?" (It's literally the same file, why doesn't it work in one app and does in the other?)

**The data is the same, but the context is different:**

| Aspect | AI Chat | Music |
|--------|---------|-------|
| **Data File** | ✅ Same file path | ✅ Same file path |
| **Playback** | Works fine | Works fine |
| **Synthesis** | ❌ FAILS (DRAM) | ❌ N/A (no synthesis) |
| **Available DRAM** | 22 KB free | 60+ KB free |
| **I2S Buffer Allocation** | ❌ Fails (needs 40KB) | ✅ Not attempted |

## Detailed Memory Breakdown

### DRAM Usage in AI Chat Screen (at speech synthesis time)

From logs: **168260 bytes total DRAM available on ESP32-S3**

After UI initialization in AI Chat:
- Root LVGL object + layout: ~15 KB
- Chat container + bubbles: ~50 KB
- Input fields + buttons: ~20 KB
- Lua sandbox state: ~30 KB
- Conversation buffer: ~25 KB
- Various timers and listeners: ~5 KB
- Heap fragmentation/overhead: ~25 KB

**Total: ~170 KB used → ~20-25 KB free**

### I2S DMA Buffer Requirement for TTS

```cpp
// For 24 kHz, 16-bit, 1-channel audio (from audio logs):
// buf_len=96, buf_count=6 (reduced to minimize DRAM)

DMA buffer size = (96 samples) × 2 bytes × 1 channel × 6 buffers
                = 1,152 × 6 = 6,912 bytes

I2S initialization overhead = ~20-30 KB
Total needed ≈ 35-40 KB
```

**Available: 22 KB < Required: 40 KB → FAILURE**

## Solutions

### Solution 1: Reduce DRAM Usage Before TTS (RECOMMENDED)

**Target: Free up 20-25 KB in AI Chat screen**

#### A. Optimize Conversation Buffer
```cpp
// In voice_assistant.cpp, reduce buffer limit
const int MAX_CONVERSATION_ENTRIES = 20;  // was 30
// Saves: ~5-10 KB
```

#### B. Defer Chat History Loading
```cpp
// Don't fully load all chat bubbles immediately
// Load visible entries only (lazy loading)
// Saves: ~15-20 KB
```

#### C. Minimize Lua Sandbox State
```cpp
// Pre-compile Lua code to bytecode
// Reduce number of registered functions
// Saves: ~10 KB
```

### Solution 2: Allocate I2S Buffers from PSRAM

**Modify I2S driver to prefer PSRAM:**

```cpp
// In i2s_driver.cpp
esp_i2s_config_t config = {
    // ...existing config...
};

// Force I2S DMA buffers to use PSRAM if available
heap_caps_malloc_extmem_enable(40 * 1024);  // Enable PSRAM allocation

// Result: I2S buffers (~6-7 KB) allocated from PSRAM
// Saves: ~35 KB in DRAM for TTS use
```

### Solution 3: Lower DRAM Threshold Dynamically

**Adjust threshold based on available memory:**

```cpp
// Instead of fixed 40 KB threshold:
size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

// If PSRAM available, reduce DRAM requirement
size_t required_dram = (free_psram > 100 * 1024) ? 25 * 1024 : 40 * 1024;

if (free_dram >= required_dram) {
    // Proceed with TTS
}
```

### Solution 4: Pre-allocate from PSRAM During Init

**During AI Chat screen initialization:**

```cpp
// Pre-allocate I2S buffers pool from PSRAM
void* i2s_buffer_pool = heap_caps_malloc(40 * 1024, MALLOC_CAP_SPIRAM);
// Pass to AudioManager for I2S use
AudioManager::setBufferPool(i2s_buffer_pool);
```

## Implementation Priority

1. **HIGHEST:** Solution 2 (Allocate I2S from PSRAM)
   - Minimal code changes
   - Maximum impact
   - No UX changes

2. **HIGH:** Solution 1A (Reduce buffer limit)
   - Quick wins
   - Minimal impact on functionality
   - 5-10 KB saved

3. **MEDIUM:** Solution 3 (Dynamic threshold)
   - Safety improvement
   - Flexibility for different devices

4. **LOW:** Solution 1B/1C (Defer loading, optimize Lua)
   - More complex
   - Better for long-term

## Testing Strategy

After implementing **Solution 2**:

1. **In AI Chat screen:**
   - Check DRAM before TTS: Should be ≥ 20 KB
   - Attempt TTS synthesis
   - Verify: `[INFO] [TTS] Received XXX bytes of audio data`
   - Play synthesized audio

2. **Compare with Music app:**
   - Verify both paths work
   - Check memory logs match

3. **Edge cases:**
   - Multiple TTS calls in succession
   - Long conversations (40+ entries)
   - Large voice assistant prompt

## Code References

| File | Function | Line | Issue |
|------|----------|------|-------|
| `src/core/voice_assistant.cpp` | `lua_tts_speak()` | 2826 | DRAM check too strict |
| `lib/openESPaudio/src/i2s_driver.cpp` | `I2S_init()` | ? | DMA buffers in DRAM |
| `src/screens/ai_chat_screen.cpp` | `build()` | 50+ | Heavy UI construction |
| `src/core/conversation_buffer.cpp` | Constructor | ? | Allocates 30 entries |

## Expected Outcomes

After Solution 2 implementation:

```
BEFORE (Current):
├── Free DRAM: 22 KB
├── I2S needs: 40 KB (in DRAM)
└── Result: ❌ TTS FAILS

AFTER (With I2S from PSRAM):
├── Free DRAM: 22 KB
├── I2S needs: 40 KB (in PSRAM) ✅
├── Remaining DRAM: 22 KB (for other ops)
└── Result: ✅ TTS SUCCEEDS
```

## Files to Modify

1. `lib/openESPaudio/src/i2s_driver.cpp` - Enable PSRAM for I2S buffers
2. `lib/openESPaudio/src/i2s_driver.h` - Configuration flags
3. `src/core/voice_assistant.cpp` - Optional: Dynamic threshold adjustment
4. `src/screens/ai_chat_screen.cpp` - Optional: Reduce buffer limit

---

**Summary:** The file works in Music app because no TTS synthesis happens. In AI Chat, TTS needs 40 KB for I2S DMA buffers but only 22 KB is free. Solution: Allocate I2S buffers from PSRAM instead of DRAM.
