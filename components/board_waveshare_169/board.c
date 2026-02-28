#include "board.h"

#include "driver/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

#define PIN_LCD_DC 4
#define PIN_LCD_CS 5
#define PIN_LCD_SCK 6
#define PIN_LCD_MOSI 7
#define PIN_LCD_RST 8
#define PIN_LCD_BL 15

#define PIN_I2C_SCL 10
#define PIN_I2C_SDA 11
#define PIN_TOUCH_RST 13
#define PIN_TOUCH_INT 14
#define PIN_BAT_ADC 1
#define PIN_BUZZER 33
#define PIN_SYS_EN 35
#define PIN_SYS_OUT 36

#define LCD_H_RES 240
#define LCD_V_RES 280

static const char *TAG = "board169";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static i2c_master_bus_handle_t s_i2c_bus;
static adc_oneshot_unit_handle_t s_adc;
static bool s_touch_ok;

static void board_sys_safe_init(void) {
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_SYS_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);
    gpio_set_level(PIN_SYS_EN, 1);

    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_SYS_OUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&in);
}

static esp_err_t board_init_backlight(void) {
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&tcfg), TAG, "ledc timer");

    ledc_channel_config_t ccfg = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ccfg), TAG, "ledc channel");
    board_backlight_set(CONFIG_BOARD_BACKLIGHT_DEFAULT_PERCENT);
    return ESP_OK;
}

void board_backlight_set(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = (1023 * percent) / 100;
    if (CONFIG_BOARD_BACKLIGHT_INVERT) {
        duty = 1023 - duty;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static esp_err_t board_init_lcd(void) {
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(CONFIG_BOARD_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi init");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = CONFIG_BOARD_LCD_SPI_FREQ_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)CONFIG_BOARD_LCD_SPI_HOST, &io_config, &io_handle), TAG, "panel io");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel), TAG, "new panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
#if CONFIG_BOARD_LCD_ROT_0
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_mirror(s_panel, true, false);
#elif CONFIG_BOARD_LCD_ROT_90
    esp_lcd_panel_swap_xy(s_panel, true);
    esp_lcd_panel_mirror(s_panel, true, true);
#elif CONFIG_BOARD_LCD_ROT_180
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_mirror(s_panel, false, true);
#elif CONFIG_BOARD_LCD_ROT_270
    esp_lcd_panel_swap_xy(s_panel, true);
    esp_lcd_panel_mirror(s_panel, false, false);
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on");
    return ESP_OK;
}

static esp_err_t board_init_i2c(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = CONFIG_BOARD_I2C_PORT,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "new i2c bus");
    return ESP_OK;
}

static esp_err_t board_init_touch(void) {
#if CONFIG_BOARD_ENABLE_TOUCH
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = CONFIG_BOARD_I2C_FREQ_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io), TAG, "touch io");

    esp_lcd_touch_config_t cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = CONFIG_BOARD_TOUCH_USE_INT ? PIN_TOUCH_INT : -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
#if CONFIG_BOARD_LCD_ROT_0
        .flags.swap_xy = 0,
        .flags.mirror_x = 1,
        .flags.mirror_y = 0,
#elif CONFIG_BOARD_LCD_ROT_90
        .flags.swap_xy = 1,
        .flags.mirror_x = 1,
        .flags.mirror_y = 1,
#elif CONFIG_BOARD_LCD_ROT_180
        .flags.swap_xy = 0,
        .flags.mirror_x = 0,
        .flags.mirror_y = 1,
#else
        .flags.swap_xy = 1,
        .flags.mirror_x = 0,
        .flags.mirror_y = 0,
#endif
    };
    esp_err_t err = esp_lcd_touch_new_i2c_cst816s(tp_io, &cfg, &s_touch);
    if (err == ESP_OK) {
        s_touch_ok = true;
        ESP_LOGI(TAG, "Touch probe: OK");
    } else {
        ESP_LOGW(TAG, "Touch probe failed: %s", esp_err_to_name(err));
    }
#endif
    return ESP_OK;
}

static esp_err_t board_init_battery(void) {
#if CONFIG_BOARD_ENABLE_BATTERY
    adc_oneshot_unit_init_cfg_t init_cfg = {.unit_id = ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_cfg, &s_adc), TAG, "adc unit");
    adc_oneshot_chan_cfg_t cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &cfg), TAG, "adc ch");
#endif
    return ESP_OK;
}

void board_buzzer_set(bool on) {
#if CONFIG_BOARD_ENABLE_BUZZER
    gpio_set_level(PIN_BUZZER, on ? 1 : 0);
#else
    (void)on;
#endif
}

esp_err_t board_battery_read_mv(int *out_mv) {
#if CONFIG_BOARD_ENABLE_BATTERY
    if (!out_mv || !s_adc) {
        return ESP_ERR_INVALID_ARG;
    }
    int raw = 0;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw), TAG, "adc read");
    int mv = (raw * 3300) / 4095;
    mv = (mv * CONFIG_BOARD_BAT_DIV_NUM) / CONFIG_BOARD_BAT_DIV_DEN;
    *out_mv = mv;
    return ESP_OK;
#else
    (void)out_mv;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_lcd_panel_handle_t board_lcd_panel(void) { return s_panel; }
esp_lcd_touch_handle_t board_touch(void) { return s_touch; }
bool board_touch_present(void) { return s_touch_ok && s_touch != NULL; }

esp_err_t board_init(void) {
    board_sys_safe_init();
    ESP_LOGI(TAG, "SYS_EN asserted, SYS_OUT sampled for safety");

#if CONFIG_BOARD_ENABLE_BUZZER
    gpio_config_t buzz_cfg = {.pin_bit_mask = (1ULL << PIN_BUZZER), .mode = GPIO_MODE_OUTPUT};
    gpio_config(&buzz_cfg);
    board_buzzer_set(false);
#endif

    ESP_RETURN_ON_ERROR(board_init_backlight(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(board_init_lcd(), TAG, "lcd");
    ESP_RETURN_ON_ERROR(board_init_i2c(), TAG, "i2c");
    ESP_RETURN_ON_ERROR(board_init_touch(), TAG, "touch");
    ESP_RETURN_ON_ERROR(board_init_battery(), TAG, "battery");
    return ESP_OK;
}
