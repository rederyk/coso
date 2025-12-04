# 💻 Useful Code Snippets

## Dashboard Widget Pattern
```cpp
// Base widget creation
container = lv_obj_create(parent);
lv_obj_set_size(container, lv_pct(100), 100);
lv_obj_set_style_radius(container, 12, 0);
lv_obj_set_style_bg_color(container, lv_color_hex(0x4a90e2), 0);

// LVGL refresh timer
refresh_timer = lv_timer_create([](lv_timer_t* timer) {
    self->update();
}, 30000, this);  // 30 seconds
```

## LVGL Thread Safety
```cpp
// Safe UI updates from background threads
const bool already_owned = lvgl_mutex_is_owned_by_current_task();
bool lock_acquired = already_owned;
if (!already_owned) {
    lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
}
if (lock_acquired) {
    // Update UI elements
    lv_label_set_text(label, "Updated");
    if (!already_owned) {
        lvgl_mutex_unlock();
    }
}
```

## WebDataManager Usage
```cpp
// Fetch data once
web_data.fetchOnce("https://api.example.com/data", "data.json");

// Fetch with scheduling
web_data.fetchScheduled("https://api.example.com/data", "data.json", 60);

// Read cached data
std::string data = web_data.readData("data.json");
```

## JSON Parsing with cJSON
```cpp
cJSON* root = cJSON_Parse(json_string.c_str());
if (root) {
    cJSON* temperature = cJSON_GetObjectItem(root, "temperature");
    if (temperature && cJSON_IsNumber(temperature)) {
        float temp = temperature->valuedouble;
    }
    cJSON_Delete(root);
}
