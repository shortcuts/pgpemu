#include "battery.h"

#include "sdkconfig.h"

#ifndef CONFIG_PGPEMU_BATTERY_FULL_MV
#define CONFIG_PGPEMU_BATTERY_FULL_MV 4200
#endif
#ifndef CONFIG_PGPEMU_BATTERY_EMPTY_MV
#define CONFIG_PGPEMU_BATTERY_EMPTY_MV 3300
#endif

uint8_t battery_mv_to_percent(uint32_t mv, uint32_t empty_mv, uint32_t full_mv) {
    if (full_mv <= empty_mv || mv <= empty_mv)
        return 0;
    if (mv >= full_mv)
        return 100;
    return (uint8_t)((uint64_t)(mv - empty_mv) * 100 / (full_mv - empty_mv));
}

#ifdef CONFIG_PGPEMU_BATTERY_ENABLED

#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t adc_handle;
static bool adc_ready;

void init_battery(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_ready = adc_oneshot_new_unit(&unit_cfg, &adc_handle) == ESP_OK
                && adc_oneshot_config_channel(adc_handle, CONFIG_PGPEMU_BATTERY_ADC_CHANNEL, &chan_cfg) == ESP_OK;
}

// shortcut: uncalibrated ADC, ±5 percent, upgrade to adc_cali if users report drift
uint32_t battery_get_mv(void) {
    int raw;
    if (!adc_ready || adc_oneshot_read(adc_handle, CONFIG_PGPEMU_BATTERY_ADC_CHANNEL, &raw) != ESP_OK)
        return 0;
    return (raw * 3100 / 4095) * CONFIG_PGPEMU_BATTERY_DIVIDER_RATIO;
}

#else

void init_battery(void) {
}
uint32_t battery_get_mv(void) {
    return 0;
}

#endif

uint8_t battery_get_percent(void) {
    return battery_mv_to_percent(battery_get_mv(), CONFIG_PGPEMU_BATTERY_EMPTY_MV, CONFIG_PGPEMU_BATTERY_FULL_MV);
}
