/*
 * Holds the TPS65 (IQS550) trackpad in reset while the half sleeps (task 08
 * step 6, D14).
 *
 * ZMK suspends every PM-capable device before deep sleep and soft-off
 * (zmk_pm_suspend_devices in app/src/pm.c). On suspend this device drives the
 * pad's RST line active; the nRF keeps the pin level while it is off, so the
 * pad stays in reset instead of running in its own low-power mode (task 08b
 * measures the difference). Waking up restarts
 * the half, and the trackpad driver resets and sets up the pad as usual.
 * The pin is not touched at start-up: the driver owns it while running.
 */

#define DT_DRV_COMPAT zmk_tps65_sleep_reset

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(tps65_sleep_reset, CONFIG_ZMK_LOG_LEVEL);

struct tps65_sleep_reset_config {
    struct gpio_dt_spec reset;
};

static int tps65_sleep_reset_init(const struct device *dev) {
    const struct tps65_sleep_reset_config *cfg = dev->config;

    if (!gpio_is_ready_dt(&cfg->reset)) {
        LOG_ERR("RST GPIO not ready");
        return -ENODEV;
    }
    return 0;
}

static int tps65_sleep_reset_pm_action(const struct device *dev, enum pm_device_action action) {
    const struct tps65_sleep_reset_config *cfg = dev->config;

    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        // Pad into reset (RST is active low, the flag comes from devicetree).
        return gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_ACTIVE);
    case PM_DEVICE_ACTION_RESUME:
        // Only reached if going to sleep failed: let the pad run again.
        return gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_INACTIVE);
    default:
        return -ENOTSUP;
    }
}

#define TPS65_SLEEP_RESET_INIT(n)                                                                  \
    static const struct tps65_sleep_reset_config tps65_sleep_reset_config_##n = {                \
        .reset = GPIO_DT_SPEC_INST_GET(n, reset_gpios),                                            \
    };                                                                                             \
    PM_DEVICE_DT_INST_DEFINE(n, tps65_sleep_reset_pm_action);                                      \
    DEVICE_DT_INST_DEFINE(n, tps65_sleep_reset_init, PM_DEVICE_DT_INST_GET(n), NULL,              \
                          &tps65_sleep_reset_config_##n, POST_KERNEL,                              \
                          CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(TPS65_SLEEP_RESET_INIT)
