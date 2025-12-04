#pragma once

#include "widgets/dashboard_widget.h"
#include "core/web_data_manager.h"
#include <string>

class WeatherWidget : public DashboardWidget {
public:
    WeatherWidget();
    ~WeatherWidget() override;

    void create(lv_obj_t* parent) override;
    void update() override;

private:
    struct WeatherData {
        float temperature = 0.0f;
        int weather_code = 0;
        float windspeed = 0.0f;
        std::string condition;
        bool valid = false;
    };

    void fetchWeatherData();
    void parseWeatherJson(const std::string& json_data);
    std::string getWeatherIcon(int weather_code) const;
    std::string getWeatherDescription(int weather_code) const;

    lv_obj_t* temperature_label = nullptr;
    lv_obj_t* condition_label = nullptr;
    lv_obj_t* icon_label = nullptr;
    lv_obj_t* container = nullptr;
    lv_timer_t* refresh_timer = nullptr;

    WeatherData current_weather;
    WebDataManager& web_data;
};
