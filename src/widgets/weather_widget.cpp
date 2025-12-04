#include "widgets/weather_widget.h"
#include <lvgl.h>
#include <cJSON.h>
#include <string>

WeatherWidget::WeatherWidget() : web_data(WebDataManager::getInstance()), refresh_timer(nullptr) {}

WeatherWidget::~WeatherWidget() {
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void WeatherWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), 100);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x4a90e2), 0);  // Blue background for weather
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 12, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Weather icon (top)
    icon_label = lv_label_create(container);
    lv_label_set_text_static(icon_label, "🌤️");  // Default sunny icon
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xffe66d), 0);

    // Temperature label (center)
    temperature_label = lv_label_create(container);
    lv_label_set_text_static(temperature_label, "--°C");
    lv_obj_set_style_text_font(temperature_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(temperature_label, lv_color_hex(0xffffff), 0);

    // Condition label (bottom)
    condition_label = lv_label_create(container);
    lv_label_set_text_static(condition_label, "Loading...");
    lv_obj_set_style_text_font(condition_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(condition_label, lv_color_hex(0xcccccc), 0);

    // Fetch initial weather data
    fetchWeatherData();

    // Create timer for weather updates (every 30 minutes)
    refresh_timer = lv_timer_create([](lv_timer_t* timer) {
        if (!timer || !timer->user_data) return;
        WeatherWidget* self = static_cast<WeatherWidget*>(timer->user_data);
        self->fetchWeatherData();
    }, 30 * 60 * 1000, this);  // 30 minutes in milliseconds
}

void WeatherWidget::update() {
    // Update is called by LVGL periodically, but we use the timer for weather updates
    // This method is kept for consistency with the interface
}

void WeatherWidget::fetchWeatherData() {
    // Fetch weather data from Open-Meteo API
    // Using coordinates for Milan, Italy (can be made configurable later)
    const char* url = "https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current_weather=true&windspeed_unit=ms&hourly=temperature_2m&forecast_days=1";

    auto result = web_data.fetchOnce(url, "weather.json");

    if (result.success) {
        // Read the fetched data
        std::string weather_json = web_data.readData("weather.json");
        if (!weather_json.empty()) {
            parseWeatherJson(weather_json);
        } else {
            // Set error state
            current_weather.valid = false;

            const bool already_owned = lvgl_mutex_is_owned_by_current_task();
            bool lock_acquired = already_owned;
            if (!already_owned) {
                lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
            }

            if (lock_acquired) {
                if (icon_label) lv_label_set_text(icon_label, "❓");
                if (temperature_label) lv_label_set_text(temperature_label, "--°C");
                if (condition_label) lv_label_set_text(condition_label, "No data");

                if (!already_owned) {
                    lvgl_mutex_unlock();
                }
            }
        }
    } else {
        // Set error state
        current_weather.valid = false;

        const bool already_owned = lvgl_mutex_is_owned_by_current_task();
        bool lock_acquired = already_owned;
        if (!already_owned) {
            lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
        }

        if (lock_acquired) {
            if (icon_label) lv_label_set_text(icon_label, "❌");
            if (temperature_label) lv_label_set_text(temperature_label, "--°C");
            if (condition_label) lv_label_set_text(condition_label, "Fetch failed");

            if (!already_owned) {
                lvgl_mutex_unlock();
            }
        }
    }
}

void WeatherWidget::parseWeatherJson(const std::string& json_data) {
    current_weather.valid = false;

    cJSON* root = cJSON_Parse(json_data.c_str());
    if (!root) {
        return;
    }

    // Parse current weather
    cJSON* current_weather_obj = cJSON_GetObjectItem(root, "current_weather");
    if (current_weather_obj && cJSON_IsObject(current_weather_obj)) {
        cJSON* temperature = cJSON_GetObjectItem(current_weather_obj, "temperature");
        cJSON* weathercode = cJSON_GetObjectItem(current_weather_obj, "weathercode");
        cJSON* windspeed = cJSON_GetObjectItem(current_weather_obj, "windspeed");

        if (temperature && cJSON_IsNumber(temperature)) {
            current_weather.temperature = static_cast<float>(temperature->valuedouble);
        }

        if (weathercode && cJSON_IsNumber(weathercode)) {
            current_weather.weather_code = weathercode->valueint;
            current_weather.condition = getWeatherDescription(current_weather.weather_code);
        }

        if (windspeed && cJSON_IsNumber(windspeed)) {
            current_weather.windspeed = static_cast<float>(windspeed->valuedouble);
        }

        current_weather.valid = true;
    }

    cJSON_Delete(root);

    // Update UI with parsed data
    if (current_weather.valid) {
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.1f°C", current_weather.temperature);

        std::string weather_icon = getWeatherIcon(current_weather.weather_code);

        const bool already_owned = lvgl_mutex_is_owned_by_current_task();
        bool lock_acquired = already_owned;
        if (!already_owned) {
            lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
        }

        if (lock_acquired) {
            if (icon_label) lv_label_set_text(icon_label, weather_icon.c_str());
            if (temperature_label) lv_label_set_text(temperature_label, temp_str);
            if (condition_label) lv_label_set_text(condition_label, current_weather.condition.c_str());

            if (!already_owned) {
                lvgl_mutex_unlock();
            }
        }
    }
}

std::string WeatherWidget::getWeatherIcon(int weather_code) const {
    // Open-Meteo weather codes: https://open-meteo.com/en/docs
    switch (weather_code) {
        case 0: return "☀️";  // Clear sky
        case 1: return "🌤️"; // Mainly clear
        case 2: return "⛅"; // Partly cloudy
        case 3: return "☁️"; // Overcast
        case 45: return "🌫️"; // Fog
        case 48: return "🌫️"; // Depositing rime fog
        case 51: return "🌦️"; // Light drizzle
        case 53: return "🌦️"; // Moderate drizzle
        case 55: return "🌦️"; // Dense drizzle
        case 56: return "🌨️"; // Light freezing drizzle
        case 57: return "🌨️"; // Dense freezing drizzle
        case 61: return "🌧️"; // Slight rain
        case 63: return "🌧️"; // Moderate rain
        case 65: return "🌧️"; // Heavy rain
        case 66: return "🌨️"; // Light freezing rain
        case 67: return "🌨️"; // Heavy freezing rain
        case 71: return "❄️"; // Slight snow fall
        case 73: return "❄️"; // Moderate snow fall
        case 75: return "❄️"; // Heavy snow fall
        case 77: return "❄️"; // Snow grains
        case 80: return "🌦️"; // Slight rain showers
        case 81: return "🌦️"; // Moderate rain showers
        case 82: return "🌦️"; // Violent rain showers
        case 85: return "🌨️"; // Slight snow showers
        case 86: return "🌨️"; // Heavy snow showers
        case 95: return "⛈️"; // Thunderstorm
        case 96: return "⛈️"; // Thunderstorm with slight hail
        case 99: return "⛈️"; // Thunderstorm with heavy hail
        default: return "❓";
    }
}

std::string WeatherWidget::getWeatherDescription(int weather_code) const {
    switch (weather_code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: return "Fog";
        case 48: return "Rime fog";
        case 51: return "Light drizzle";
        case 53: return "Drizzle";
        case 55: return "Dense drizzle";
        case 56: return "Light freezing drizzle";
        case 57: return "Dense freezing drizzle";
        case 61: return "Slight rain";
        case 63: return "Rain";
        case 65: return "Heavy rain";
        case 66: return "Light freezing rain";
        case 67: return "Heavy freezing rain";
        case 71: return "Light snow";
        case 73: return "Snow";
        case 75: return "Heavy snow";
        case 77: return "Snow grains";
        case 80: return "Light showers";
        case 81: return "Showers";
        case 82: return "Violent showers";
        case 85: return "Light snow showers";
        case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: return "Thunderstorm w/ hail";
        case 99: return "Heavy thunderstorm";
        default: return "Unknown";
    }
}
