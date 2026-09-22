#include "WeatherXMTFTView.h"

#if HAS_TFT && (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

#include "Throttle.h"
#include "WeatherXMModule.h"
#include "fonts.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include "images.h"
#include "lvgl.h"
#include "screens.h"
#include "styles.h"
#include "util/ILog.h"
#include <cstdio>

static constexpr lv_style_selector_t SEL_MAIN = static_cast<lv_style_selector_t>(LV_PART_MAIN);

// LVGL objects
static lv_obj_t *top_weather_panel = nullptr;
static lv_obj_t *weather_panel = nullptr;
static lv_obj_t *weather_button = nullptr;
static lv_obj_t *home_weather_button = nullptr;
static lv_obj_t *home_weather_label = nullptr;

// Top panel widgets
static lv_obj_t *top_title_label = nullptr;
static lv_obj_t *top_station_label = nullptr;
static lv_obj_t *top_status_label = nullptr;

// Toolbar controls
static lv_obj_t *toolbar_station_label = nullptr;
static lv_obj_t *btn_prev = nullptr;
static lv_obj_t *btn_next = nullptr;
static lv_obj_t *btn_units = nullptr;
static lv_obj_t *btn_units_label = nullptr;
static lv_obj_t *btn_auto = nullptr;
static lv_obj_t *btn_auto_label = nullptr;

// Temperature card
static lv_obj_t *temp_main_label = nullptr;
static lv_obj_t *temp_sub_label = nullptr;
static lv_obj_t *temp_range_label = nullptr;

// Precipitation card
static lv_obj_t *rain_rate_label = nullptr;
static lv_obj_t *rain_total_label = nullptr;

// Barometer card
static lv_obj_t *press_main_label = nullptr;
static lv_obj_t *press_sub_label = nullptr;
static lv_obj_t *press_range_label = nullptr;

// Humidity card
static lv_obj_t *hum_main_label = nullptr;
static lv_obj_t *hum_sub_label = nullptr;
static lv_obj_t *hum_range_label = nullptr;

// Solar & UV card
static lv_obj_t *solar_rad_label = nullptr;
static lv_obj_t *solar_uv_label = nullptr;

// Wind card
static lv_obj_t *wind_main_label = nullptr;
static lv_obj_t *wind_gust_label = nullptr;
static lv_obj_t *wind_dir_label = nullptr;

// Station diagnostics card
static lv_obj_t *diag_id_label = nullptr;
static lv_obj_t *diag_radio_label = nullptr;
static lv_obj_t *diag_battery_label = nullptr;

static lv_obj_t *createCard(lv_obj_t *parent, uint32_t accentHex, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 8, SEL_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x18202c), SEL_MAIN);
    lv_obj_set_style_bg_opa(card, 255, SEL_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2d3a4e), SEL_MAIN);
    lv_obj_set_style_border_width(card, 1, SEL_MAIN);
    lv_obj_set_style_pad_all(card, 8, SEL_MAIN);
    lv_obj_set_style_pad_row(card, 4, SEL_MAIN);
    lv_obj_set_style_layout(card, LV_LAYOUT_FLEX, SEL_MAIN);
    lv_obj_set_style_flex_flow(card, LV_FLEX_FLOW_COLUMN, SEL_MAIN);

    if (title && title[0]) {
        lv_obj_t *header = lv_label_create(card);
        lv_label_set_text(header, title);
        lv_obj_set_style_text_color(header, lv_color_hex(accentHex), SEL_MAIN);
        lv_obj_set_style_text_font(header, &ui_font_montserrat_14, SEL_MAIN);
    }
    return card;
}

static lv_obj_t *createToolbarButton(lv_obj_t *parent, const char *text, lv_obj_t **labelOut = nullptr)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_radius(btn, 6, SEL_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a3648), SEL_MAIN);
    lv_obj_set_style_border_width(btn, 1, SEL_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3e4f6a), SEL_MAIN);
    lv_obj_set_style_pad_hor(btn, 10, SEL_MAIN);
    lv_obj_set_style_pad_ver(btn, 4, SEL_MAIN);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), SEL_MAIN);
    lv_obj_center(lbl);

    if (labelOut) {
        *labelOut = lbl;
    }
    return btn;
}

void WeatherXMTFTView::init()
{
    ILOG_INFO("WeatherXMTFTView::init...");
    createUI();

    // 1-second LVGL periodic update timer on TFT thread
    lv_timer_create([](lv_timer_t *) { WeatherXMTFTView::update(); }, 1000, nullptr);
}

void WeatherXMTFTView::show()
{
    if (!weather_panel && objects.main_screen) {
        createUI();
    }
    if (TFTView_320x240::instance() && weather_panel && top_weather_panel) {
        update();
        TFTView_320x240::instance()->ui_set_active(weather_button, weather_panel, top_weather_panel);
    }
}

void WeatherXMTFTView::createUI()
{
    if (!objects.main_screen || weather_panel) {
        return;
    }

    // Top Weather Status Panel
    top_weather_panel = lv_obj_create(objects.main_screen);
    lv_obj_set_pos(top_weather_panel, LV_PCT(12), 0);
    lv_obj_set_size(top_weather_panel, LV_PCT(80), LV_PCT(10));
    lv_obj_add_flag(top_weather_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(top_weather_panel, LV_OBJ_FLAG_SCROLLABLE);
    add_style_top_panel_style(top_weather_panel);
    lv_obj_set_style_layout(top_weather_panel, LV_LAYOUT_FLEX, SEL_MAIN);
    lv_obj_set_style_flex_flow(top_weather_panel, LV_FLEX_FLOW_ROW, SEL_MAIN);
    lv_obj_set_style_flex_main_place(top_weather_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, SEL_MAIN);
    lv_obj_set_style_flex_cross_place(top_weather_panel, LV_FLEX_ALIGN_CENTER, SEL_MAIN);
    lv_obj_set_style_pad_hor(top_weather_panel, 12, SEL_MAIN);

    top_title_label = lv_label_create(top_weather_panel);
    lv_label_set_text(top_title_label, "WEATHERXM");
    lv_obj_set_style_text_color(top_title_label, lv_color_hex(0x67ea94), SEL_MAIN);
    lv_obj_set_style_text_font(top_title_label, &ui_font_montserrat_16, SEL_MAIN);

    top_station_label = lv_label_create(top_weather_panel);
    lv_label_set_text(top_station_label, "Scanning...");
    lv_obj_set_style_text_color(top_station_label, lv_color_hex(0xffffff), SEL_MAIN);
    lv_obj_set_style_text_font(top_station_label, &ui_font_montserrat_14, SEL_MAIN);

    top_status_label = lv_label_create(top_weather_panel);
    lv_label_set_text(top_status_label, "");
    lv_obj_set_style_text_color(top_status_label, lv_color_hex(0xa0b0c0), SEL_MAIN);
    lv_obj_set_style_text_font(top_status_label, &ui_font_montserrat_12, SEL_MAIN);

    // Main Weather Panel (Scrollable cards container)
    weather_panel = lv_obj_create(objects.main_screen);
    lv_obj_set_pos(weather_panel, LV_PCT(12), LV_PCT(10));
    lv_obj_set_size(weather_panel, LV_PCT(88), LV_PCT(90));
    lv_obj_add_flag(weather_panel, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_SCROLL_CHAIN));
    lv_obj_set_scrollbar_mode(weather_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(weather_panel, LV_DIR_VER);
    add_style_panel_style(weather_panel);
    lv_obj_set_style_border_width(weather_panel, 0, SEL_MAIN);
    lv_obj_set_style_layout(weather_panel, LV_LAYOUT_FLEX, SEL_MAIN);
    lv_obj_set_style_flex_flow(weather_panel, LV_FLEX_FLOW_COLUMN, SEL_MAIN);
    lv_obj_set_style_pad_all(weather_panel, 6, SEL_MAIN);
    lv_obj_set_style_pad_row(weather_panel, 6, SEL_MAIN);

    // Toolbar Card (Station selection and units toggle)
    lv_obj_t *toolbar = lv_obj_create(weather_panel);
    lv_obj_set_width(toolbar, LV_PCT(100));
    lv_obj_set_height(toolbar, LV_SIZE_CONTENT);
    lv_obj_remove_flag(toolbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(toolbar, 8, SEL_MAIN);
    lv_obj_set_style_bg_color(toolbar, lv_color_hex(0x202b3a), SEL_MAIN);
    lv_obj_set_style_bg_opa(toolbar, 255, SEL_MAIN);
    lv_obj_set_style_border_color(toolbar, lv_color_hex(0x35445b), SEL_MAIN);
    lv_obj_set_style_border_width(toolbar, 1, SEL_MAIN);
    lv_obj_set_style_pad_all(toolbar, 6, SEL_MAIN);
    lv_obj_set_style_layout(toolbar, LV_LAYOUT_FLEX, SEL_MAIN);
    lv_obj_set_style_flex_flow(toolbar, LV_FLEX_FLOW_ROW, SEL_MAIN);
    lv_obj_set_style_flex_main_place(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, SEL_MAIN);
    lv_obj_set_style_flex_cross_place(toolbar, LV_FLEX_ALIGN_CENTER, SEL_MAIN);

    toolbar_station_label = lv_label_create(toolbar);
    lv_label_set_text(toolbar_station_label, "Station 1/1");
    lv_obj_set_style_text_font(toolbar_station_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(toolbar_station_label, lv_color_hex(0xffffff), SEL_MAIN);

    lv_obj_t *ctrl_grp = lv_obj_create(toolbar);
    lv_obj_set_size(ctrl_grp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(ctrl_grp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ctrl_grp, 0, SEL_MAIN);
    lv_obj_set_style_border_width(ctrl_grp, 0, SEL_MAIN);
    lv_obj_set_style_pad_all(ctrl_grp, 0, SEL_MAIN);
    lv_obj_set_style_pad_column(ctrl_grp, 6, SEL_MAIN);
    lv_obj_set_style_layout(ctrl_grp, LV_LAYOUT_FLEX, SEL_MAIN);
    lv_obj_set_style_flex_flow(ctrl_grp, LV_FLEX_FLOW_ROW, SEL_MAIN);

    btn_prev = createToolbarButton(ctrl_grp, "<");
    btn_next = createToolbarButton(ctrl_grp, ">");
    btn_units = createToolbarButton(ctrl_grp, "C / F", &btn_units_label);
    btn_auto = createToolbarButton(ctrl_grp, "Auto: ON", &btn_auto_label);

    lv_obj_add_event_cb(
        btn_prev,
        [](lv_event_t *) {
            if (weatherXMModule) {
                weatherXMModule->prevStation();
                WeatherXMTFTView::update();
            }
        },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(
        btn_next,
        [](lv_event_t *) {
            if (weatherXMModule) {
                weatherXMModule->nextStation();
                WeatherXMTFTView::update();
            }
        },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(
        btn_units,
        [](lv_event_t *) {
            if (weatherXMModule) {
                weatherXMModule->toggleUnits();
                WeatherXMTFTView::update();
            }
        },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(
        btn_auto,
        [](lv_event_t *) {
            if (weatherXMModule) {
                weatherXMModule->setAutoRotate(!weatherXMModule->isAutoRotateEnabled());
                WeatherXMTFTView::update();
            }
        },
        LV_EVENT_CLICKED, nullptr);

    // 1. Temperature Card (Accent: 0xffa040 Amber)
    lv_obj_t *temp_card = createCard(weather_panel, 0xffa040, "TEMPERATURE");
    temp_main_label = lv_label_create(temp_card);
    lv_label_set_text(temp_main_label, "--.- \xc2\xb0\x43");
    lv_obj_set_style_text_font(temp_main_label, &ui_font_montserrat_20, SEL_MAIN);
    lv_obj_set_style_text_color(temp_main_label, lv_color_hex(0xffffff), SEL_MAIN);

    temp_sub_label = lv_label_create(temp_card);
    lv_label_set_text(temp_sub_label, "Feels like: --.- \xc2\xb0\x43   Dew point: --.- \xc2\xb0\x43");
    lv_obj_set_style_text_font(temp_sub_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(temp_sub_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    temp_range_label = lv_label_create(temp_card);
    lv_label_set_text(temp_range_label, "Min: --.- \xc2\xb0   Max: --.- \xc2\xb0");
    lv_obj_set_style_text_font(temp_range_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(temp_range_label, lv_color_hex(0x8898aa), SEL_MAIN);

    // 2. Precipitation Card (Accent: 0x40b0ff Sky Blue)
    lv_obj_t *rain_card = createCard(weather_panel, 0x40b0ff, "PRECIPITATION");
    rain_rate_label = lv_label_create(rain_card);
    lv_label_set_text(rain_rate_label, "Rate: -.-- mm/h");
    lv_obj_set_style_text_font(rain_rate_label, &ui_font_montserrat_16, SEL_MAIN);
    lv_obj_set_style_text_color(rain_rate_label, lv_color_hex(0xffffff), SEL_MAIN);

    rain_total_label = lv_label_create(rain_card);
    lv_label_set_text(rain_total_label, "24h Accumulation: -.-- mm");
    lv_obj_set_style_text_font(rain_total_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(rain_total_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    // 3. Barometer Card (Accent: 0x36d6c3 Teal)
    lv_obj_t *press_card = createCard(weather_panel, 0x36d6c3, "BAROMETER");
    press_main_label = lv_label_create(press_card);
    lv_label_set_text(press_main_label, "----.- hPa");
    lv_obj_set_style_text_font(press_main_label, &ui_font_montserrat_20, SEL_MAIN);
    lv_obj_set_style_text_color(press_main_label, lv_color_hex(0xffffff), SEL_MAIN);

    press_sub_label = lv_label_create(press_card);
    lv_label_set_text(press_sub_label, "BMP390 ACTIVE");
    lv_obj_set_style_text_font(press_sub_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(press_sub_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    press_range_label = lv_label_create(press_card);
    lv_label_set_text(press_range_label, "Min: ----.-   Max: ----.-");
    lv_obj_set_style_text_font(press_range_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(press_range_label, lv_color_hex(0x8898aa), SEL_MAIN);

    // 4. Humidity Card (Accent: 0x58c0ff Cyan)
    lv_obj_t *hum_card = createCard(weather_panel, 0x58c0ff, "HUMIDITY");
    hum_main_label = lv_label_create(hum_card);
    lv_label_set_text(hum_main_label, "-- %");
    lv_obj_set_style_text_font(hum_main_label, &ui_font_montserrat_20, SEL_MAIN);
    lv_obj_set_style_text_color(hum_main_label, lv_color_hex(0xffffff), SEL_MAIN);

    hum_sub_label = lv_label_create(hum_card);
    lv_label_set_text(hum_sub_label, "Comfort: Normal");
    lv_obj_set_style_text_font(hum_sub_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(hum_sub_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    hum_range_label = lv_label_create(hum_card);
    lv_label_set_text(hum_range_label, "Min: --%   Max: --%");
    lv_obj_set_style_text_font(hum_range_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(hum_range_label, lv_color_hex(0x8898aa), SEL_MAIN);

    // 5. Solar & UV Card (Accent: 0xffcc00 Gold)
    lv_obj_t *solar_card = createCard(weather_panel, 0xffcc00, "SOLAR & UV");
    solar_rad_label = lv_label_create(solar_card);
    lv_label_set_text(solar_rad_label, "Radiation: --- W/m\xc2\xb2");
    lv_obj_set_style_text_font(solar_rad_label, &ui_font_montserrat_16, SEL_MAIN);
    lv_obj_set_style_text_color(solar_rad_label, lv_color_hex(0xffffff), SEL_MAIN);

    solar_uv_label = lv_label_create(solar_card);
    lv_label_set_text(solar_uv_label, "UV Index: - (Low)");
    lv_obj_set_style_text_font(solar_uv_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(solar_uv_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    // 6. Wind Card (Accent: 0x67ea94 Lime Green)
    lv_obj_t *wind_card = createCard(weather_panel, 0x67ea94, "WIND");
    wind_main_label = lv_label_create(wind_card);
    lv_label_set_text(wind_main_label, "Speed: -.-- m/s");
    lv_obj_set_style_text_font(wind_main_label, &ui_font_montserrat_20, SEL_MAIN);
    lv_obj_set_style_text_color(wind_main_label, lv_color_hex(0xffffff), SEL_MAIN);

    wind_gust_label = lv_label_create(wind_card);
    lv_label_set_text(wind_gust_label, "Gust: -.-- m/s   Max: -.-- m/s");
    lv_obj_set_style_text_font(wind_gust_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(wind_gust_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    wind_dir_label = lv_label_create(wind_card);
    lv_label_set_text(wind_dir_label, "Direction: --- (--- \xc2\xb0)");
    lv_obj_set_style_text_font(wind_dir_label, &ui_font_montserrat_14, SEL_MAIN);
    lv_obj_set_style_text_color(wind_dir_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    // 7. Station Diagnostics Card (Accent: 0x8898aa Muted Slate)
    lv_obj_t *diag_card = createCard(weather_panel, 0x8898aa, "STATION DETAILS");
    diag_id_label = lv_label_create(diag_card);
    lv_label_set_text(diag_id_label, "Node ID: --");
    lv_obj_set_style_text_font(diag_id_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(diag_id_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    diag_radio_label = lv_label_create(diag_card);
    lv_label_set_text(diag_radio_label, "Packets: 0   RSSI: 0 dBm   SNR: 0.0 dB");
    lv_obj_set_style_text_font(diag_radio_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(diag_radio_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    diag_battery_label = lv_label_create(diag_card);
    lv_label_set_text(diag_battery_label, "Battery: --");
    lv_obj_set_style_text_font(diag_battery_label, &ui_font_montserrat_12, SEL_MAIN);
    lv_obj_set_style_text_color(diag_battery_label, lv_color_hex(0xc8d6e5), SEL_MAIN);

    // Add Weather button to left navigation bar
    if (objects.button_panel) {
        weather_button = lv_button_create(objects.button_panel);
        lv_obj_set_pos(weather_button, 0, 0);

        lv_display_t *disp = lv_display_get_default();
        int32_t h = disp ? lv_display_get_horizontal_resolution(disp) : 320;
        int32_t v = disp ? lv_display_get_vertical_resolution(disp) : 240;
        int btn_size = 36;
        if (h > 320 && v > 320) {
            btn_size = 58;
            lv_obj_add_flag(objects.button_panel, LV_OBJ_FLAG_SCROLLABLE);
            for (int i = 0; i < lv_obj_get_child_count(objects.button_panel); i++) {
                lv_obj_set_size(lv_obj_get_child(objects.button_panel, i), btn_size, btn_size);
            }
        }
        lv_obj_set_size(weather_button, btn_size, btn_size);
        lv_obj_add_flag(weather_button, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_remove_flag(weather_button, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLL_CHAIN_HOR |
                                                                      LV_OBJ_FLAG_SCROLL_CHAIN_VER));
        add_style_main_button_style(weather_button);
        lv_obj_set_style_border_width(weather_button, 0, SEL_MAIN);
        lv_obj_set_style_bg_image_src(weather_button, &img_node_sensor_image, SEL_MAIN);

        lv_obj_add_event_cb(
            weather_button,
            [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                    WeatherXMTFTView::show();
                }
            },
            LV_EVENT_CLICKED, nullptr);
    }

    // Add Weather tile to Home screen container
    if (objects.home_container) {
        home_weather_button = lv_button_create(objects.home_container);
        lv_obj_set_pos(home_weather_button, 0, 0);
        lv_obj_set_size(home_weather_button, 36, 36);
        lv_obj_add_flag(home_weather_button, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_remove_flag(home_weather_button,
                           static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
        add_style_home_button_style(home_weather_button);
        lv_obj_set_style_bg_image_src(home_weather_button, &img_node_sensor_image, SEL_MAIN);

        home_weather_label = lv_label_create(objects.home_container);
        lv_obj_set_size(home_weather_label, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_text_line_space(home_weather_label, -2, SEL_MAIN);
        lv_label_set_text(home_weather_label, "WeatherXM: scanning...");

        auto clickHandler = [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                WeatherXMTFTView::show();
            }
        };
        lv_obj_add_event_cb(home_weather_button, clickHandler, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_flag(home_weather_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(home_weather_label, clickHandler, LV_EVENT_CLICKED, nullptr);
    }

    // Add Weather button to Settings -> Tools tab
    if (objects.tab_page_tools) {
        lv_obj_t *tools_wxm_btn = lv_button_create(objects.tab_page_tools);
        lv_obj_set_pos(tools_wxm_btn, 0, 0);
        lv_obj_set_size(tools_wxm_btn, LV_PCT(95), 30);
        add_style_settings_button_style(tools_wxm_btn);
        lv_obj_set_style_align(tools_wxm_btn, LV_ALIGN_TOP_MID, SEL_MAIN);
        lv_obj_set_style_shadow_width(tools_wxm_btn, 0, SEL_MAIN);
        lv_obj_set_style_bg_color(tools_wxm_btn, lv_color_hex(0x4db270), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(tools_wxm_btn, lv_color_hex(0x015114), LV_PART_MAIN | LV_STATE_PRESSED);

        lv_obj_t *tools_wxm_label = lv_label_create(tools_wxm_btn);
        lv_obj_set_pos(tools_wxm_label, 0, 0);
        lv_obj_set_size(tools_wxm_label, LV_PCT(100), LV_SIZE_CONTENT);
        lv_label_set_long_mode(tools_wxm_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_align(tools_wxm_label, LV_ALIGN_CENTER, SEL_MAIN);
        lv_label_set_text(tools_wxm_label, "WeatherXM Telemetry");

        lv_obj_add_event_cb(
            tools_wxm_btn,
            [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                    WeatherXMTFTView::show();
                }
            },
            LV_EVENT_CLICKED, nullptr);
    }

    updateTelemetryUI();
}

void WeatherXMTFTView::update()
{
    if (!weather_panel && objects.main_screen) {
        createUI();
    }
    updateTelemetryUI();
}

void WeatherXMTFTView::updateTelemetryUI()
{
    if (!weatherXMModule || !weather_panel) {
        return;
    }

    weatherxm::WeatherData data;
    weatherXMModule->getWeatherDataCopy(data);
    bool imperial = weatherXMModule->isImperial();
    bool autoRotate = weatherXMModule->isAutoRotateEnabled();
    size_t poolSize = weatherXMModule->getActiveStationCount();
    size_t poolIdx = weatherXMModule->getCurrentPoolIndex();
    bool isFav = weatherXMModule->isCurrentStationFavorite();

    // 1. Update Top & Toolbar Labels
    char topTitle[32];
    snprintf(topTitle, sizeof(topTitle), "%s%s", isFav ? "\xe2\x98\x85 " : "", "WEATHERXM");
    if (top_title_label) {
        lv_label_set_text(top_title_label, topTitle);
    }

    char stationBuf[48];
    if (data.has_station_data && data.station_name[0]) {
        if (poolSize > 1) {
            snprintf(stationBuf, sizeof(stationBuf), "%s%s (%u/%u)", isFav ? "[FAV] " : "", data.station_name,
                     (unsigned int)(poolIdx + 1), (unsigned int)poolSize);
        } else {
            snprintf(stationBuf, sizeof(stationBuf), "%s%s", isFav ? "[FAV] " : "", data.station_name);
        }
    } else {
        snprintf(stationBuf, sizeof(stationBuf), "%s", data.has_bmp390 ? "BMP390 ACTIVE" : "Scanning...");
    }

    if (top_station_label) {
        lv_label_set_text(top_station_label, stationBuf);
    }
    if (toolbar_station_label) {
        lv_label_set_text(toolbar_station_label, stationBuf);
    }

    if (top_status_label) {
        char statusBuf[32];
        if (data.has_station_data) {
            snprintf(statusBuf, sizeof(statusBuf), "#%lu pkts", (unsigned long)data.packet_count);
        } else {
            snprintf(statusBuf, sizeof(statusBuf), "%s", data.has_bmp390 ? "LOCAL SENSOR" : "STANDBY");
        }
        lv_label_set_text(top_status_label, statusBuf);
    }

    // Units & Auto-rotate button texts
    if (btn_units_label) {
        lv_label_set_text(btn_units_label, imperial ? "\xc2\xb0\x46 (Imp)" : "\xc2\xb0\x43 (Met)");
    }
    if (btn_auto_label) {
        lv_label_set_text(btn_auto_label, autoRotate ? "Auto: ON" : "Auto: OFF");
    }

    // Unit strings
    const char *tempUnit = imperial ? "\xc2\xb0\x46" : "\xc2\xb0\x43";
    const char *rainUnit = imperial ? "in" : "mm";
    const char *rateUnit = imperial ? "in/h" : "mm/h";
    const char *pressUnit = imperial ? "inHg" : "hPa";
    const char *speedUnit = imperial ? "mph" : "m/s";

    // 2. Temperature Card
    float disp_temp = imperial ? weatherxm::WeatherData::toFahrenheit(data.temperature) : data.temperature;
    float disp_fl = imperial ? weatherxm::WeatherData::toFahrenheit(data.feels_like) : data.feels_like;
    float disp_dp = imperial ? weatherxm::WeatherData::toFahrenheit(data.dew_point) : data.dew_point;

    char tempBuf[32];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f %s", disp_temp, tempUnit);
    if (temp_main_label) {
        lv_label_set_text(temp_main_label, tempBuf);
    }

    char flBuf[64];
    snprintf(flBuf, sizeof(flBuf), "Feels like: %.1f %s   Dew point: %.1f %s", disp_fl, tempUnit, disp_dp, tempUnit);
    if (temp_sub_label) {
        lv_label_set_text(temp_sub_label, flBuf);
    }

    if (temp_range_label) {
        if (data.temp_min < 900.0f && data.temp_max > -900.0f) {
            float disp_min = imperial ? weatherxm::WeatherData::toFahrenheit(data.temp_min) : data.temp_min;
            float disp_max = imperial ? weatherxm::WeatherData::toFahrenheit(data.temp_max) : data.temp_max;
            char mmBuf[48];
            snprintf(mmBuf, sizeof(mmBuf), "Min: %.1f\xc2\xb0   Max: %.1f\xc2\xb0", disp_min, disp_max);
            lv_label_set_text(temp_range_label, mmBuf);
        } else {
            lv_label_set_text(temp_range_label, "Min: --.- \xc2\xb0   Max: --.- \xc2\xb0");
        }
    }

    // 3. Precipitation Card
    float disp_rate = imperial ? weatherxm::WeatherData::toInches(data.precipitation_rate) : data.precipitation_rate;
    float disp_acc = imperial ? weatherxm::WeatherData::toInches(data.precipitation_accum) : data.precipitation_accum;

    char rateBuf[32];
    snprintf(rateBuf, sizeof(rateBuf), "Rate: %.2f %s", disp_rate, rateUnit);
    if (rain_rate_label) {
        lv_label_set_text(rain_rate_label, rateBuf);
    }

    char accBuf[32];
    snprintf(accBuf, sizeof(accBuf), "24h Accumulation: %.2f %s", disp_acc, rainUnit);
    if (rain_total_label) {
        lv_label_set_text(rain_total_label, accBuf);
    }

    // 4. Barometer Card
    float disp_press = imperial ? weatherxm::WeatherData::toInHg(data.barometric_pressure) : data.barometric_pressure;
    char pressBuf[32];
    snprintf(pressBuf, sizeof(pressBuf), "%.1f %s", disp_press, pressUnit);
    if (press_main_label) {
        lv_label_set_text(press_main_label, pressBuf);
    }

    if (press_sub_label) {
        lv_label_set_text(press_sub_label,
                          data.has_bmp390 ? "BMP390 ACTIVE" : (data.has_station_data ? "Station Sensor" : "No Sensor"));
    }

    if (press_range_label) {
        if (data.press_min < 9000.0f && data.press_max > -9000.0f) {
            float disp_pmin = imperial ? weatherxm::WeatherData::toInHg(data.press_min) : data.press_min;
            float disp_pmax = imperial ? weatherxm::WeatherData::toInHg(data.press_max) : data.press_max;
            char pExtBuf[48];
            snprintf(pExtBuf, sizeof(pExtBuf), "Min: %.1f   Max: %.1f", disp_pmin, disp_pmax);
            lv_label_set_text(press_range_label, pExtBuf);
        } else {
            lv_label_set_text(press_range_label, "Min: ----.-   Max: ----.-");
        }
    }

    // 5. Humidity Card
    char humBuf[16];
    snprintf(humBuf, sizeof(humBuf), "%.0f %%", data.humidity);
    if (hum_main_label) {
        lv_label_set_text(hum_main_label, humBuf);
    }

    if (hum_sub_label) {
        const char *comfort = "Normal";
        if (data.humidity > 0.0f) {
            if (data.humidity < 30.0f)
                comfort = "Dry";
            else if (data.humidity > 70.0f)
                comfort = "Humid";
        }
        char comfBuf[32];
        snprintf(comfBuf, sizeof(comfBuf), "Comfort: %s", comfort);
        lv_label_set_text(hum_sub_label, comfBuf);
    }

    if (hum_range_label) {
        if (data.hum_min < 900.0f && data.hum_max > -900.0f) {
            char hExtBuf[32];
            snprintf(hExtBuf, sizeof(hExtBuf), "Min: %.0f%%   Max: %.0f%%", data.hum_min, data.hum_max);
            lv_label_set_text(hum_range_label, hExtBuf);
        } else {
            lv_label_set_text(hum_range_label, "Min: --%   Max: --%");
        }
    }

    // 6. Solar & UV Card
    char radBuf[32];
    snprintf(radBuf, sizeof(radBuf), "Radiation: %.0f W/m\xc2\xb2", data.solar_radiation);
    if (solar_rad_label) {
        lv_label_set_text(solar_rad_label, radBuf);
    }

    if (solar_uv_label) {
        const char *uvText = "Low";
        if (data.uv_index >= 3 && data.uv_index <= 5)
            uvText = "Moderate";
        else if (data.uv_index >= 6 && data.uv_index <= 7)
            uvText = "High";
        else if (data.uv_index >= 8 && data.uv_index <= 10)
            uvText = "Very High";
        else if (data.uv_index >= 11)
            uvText = "Extreme";

        char uvBuf[32];
        snprintf(uvBuf, sizeof(uvBuf), "UV Index: %u (%s)", data.uv_index, uvText);
        lv_label_set_text(solar_uv_label, uvBuf);
    }

    // 7. Wind Card
    float disp_speed = imperial ? weatherxm::WeatherData::toMph(data.wind_speed) : data.wind_speed;
    float disp_gust = imperial ? weatherxm::WeatherData::toMph(data.wind_gust) : data.wind_gust;
    float disp_gmax = imperial ? weatherxm::WeatherData::toMph(data.wind_gust_max) : data.wind_gust_max;

    char speedBuf[32];
    snprintf(speedBuf, sizeof(speedBuf), "Speed: %.1f %s", disp_speed, speedUnit);
    if (wind_main_label) {
        lv_label_set_text(wind_main_label, speedBuf);
    }

    char gustBuf[48];
    snprintf(gustBuf, sizeof(gustBuf), "Gust: %.1f %s   Max: %.1f %s", disp_gust, speedUnit, disp_gmax, speedUnit);
    if (wind_gust_label) {
        lv_label_set_text(wind_gust_label, gustBuf);
    }

    char dirBuf[32];
    snprintf(dirBuf, sizeof(dirBuf), "Direction: %s (%u\xc2\xb0)", weatherxm::WeatherData::degreesToCardinal(data.wind_direction),
             data.wind_direction);
    if (wind_dir_label) {
        lv_label_set_text(wind_dir_label, dirBuf);
    }

    // 8. Station Diagnostics Card
    if (diag_id_label) {
        char idBuf[32];
        if (data.station_id != 0) {
            snprintf(idBuf, sizeof(idBuf), "Node ID: !%08lx", (unsigned long)data.station_id);
        } else {
            snprintf(idBuf, sizeof(idBuf), "Node ID: Local Node");
        }
        lv_label_set_text(diag_id_label, idBuf);
    }

    if (diag_radio_label) {
        char radDiagBuf[64];
        snprintf(radDiagBuf, sizeof(radDiagBuf), "Packets: %lu   RSSI: %d dBm   SNR: %.1f dB", (unsigned long)data.packet_count,
                 data.rssi, data.snr);
        lv_label_set_text(diag_radio_label, radDiagBuf);
    }

    if (diag_battery_label) {
        char batBuf[32];
        if (data.battery_mv > 0) {
            snprintf(batBuf, sizeof(batBuf), "Battery: %.2f V", data.battery_mv / 1000.0f);
        } else {
            snprintf(batBuf, sizeof(batBuf), "Battery: N/A");
        }
        lv_label_set_text(diag_battery_label, batBuf);
    }

    // 9. Update Home Screen Weather Tile
    if (home_weather_label) {
        char homeBuf[64];
        if (data.has_station_data || data.has_bmp390) {
            snprintf(homeBuf, sizeof(homeBuf), "WeatherXM: %.1f%s, %.0f%% RH\n%.1f %s", disp_temp, tempUnit, data.humidity,
                     disp_press, pressUnit);
        } else {
            snprintf(homeBuf, sizeof(homeBuf), "WeatherXM: scanning...");
        }
        lv_label_set_text(home_weather_label, homeBuf);
    }
}

#endif
