#include "battery.h"
#include "sdkconfig.h"

#ifdef CONFIG_PGPEMU_BATTERY_ENABLED

#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t adc_handle;

void init_battery(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, CONFIG_PGPEMU_BATTERY_ADC_CHANNEL, &chan_cfg);
}

uint32_t battery_get_mv(void) {
    int raw;
    adc_oneshot_read(adc_handle, CONFIG_PGPEMU_BATTERY_ADC_CHANNEL, &raw);
    return (raw * 3100 / 4095) * CONFIG_PGPEMU_BATTERY_DIVIDER_RATIO;
}

uint8_t battery_get_percent(void) {
    uint32_t mv = battery_get_mv();
    if (mv >= CONFIG_PGPEMU_BATTERY_FULL_MV) return 100;
    if (mv <= CONFIG_PGPEMU_BATTERY_EMPTY_MV) return 0;
    return (uint8_t)((mv - CONFIG_PGPEMU_BATTERY_EMPTY_MV) * 100 /
                     (CONFIG_PGPEMU_BATTERY_FULL_MV - CONFIG_PGPEMU_BATTERY_EMPTY_MV));
}

#else

void init_battery(void) {}
uint32_t battery_get_mv(void) { return 0; }
uint8_t battery_get_percent(void) { return 0; }

#endif
