/*
 * Dongle screen toggle key (task 09, D40/D41).
 *
 * The screen is off at every start; the key flips it on and off. While on,
 * ZMK's own idle blanking works as before (dark 30 s after the last activity).
 *
 * One job, "apply", sets the screen from the flag when it runs: off calls
 * ZMK's own blank_display_cb (panel off and the redraw timer stopped, as after
 * idle), on calls unblank_display_cb. It runs on ZMK's display work queue and
 * is always queued in two hops, so it runs after any job ZMK queued for the
 * same event: ZMK wakes the screen from its system work queue, which the
 * display thread can't interrupt.
 */

#define DT_DRV_COMPAT zmk_behavior_screen_toggle

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/activity.h>
#include <zmk/behavior.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// ZMK's own screen off/on jobs (app/src/display/main.c, not in a header).
void blank_display_cb(struct k_work *work);
void unblank_display_cb(struct k_work *work);

// Off at every start (D41).
static atomic_t screen_off = ATOMIC_INIT(1);

static void apply_cb(struct k_work *work) {
    if (atomic_get(&screen_off)) {
        blank_display_cb(work);
    } else {
        unblank_display_cb(work);
    }
}

K_WORK_DEFINE(apply_work, apply_cb);

// First hop: queue "apply" at the end of the display queue, behind ZMK's job.
static void apply_hop_cb(struct k_work *work) {
    k_work_submit_to_queue(zmk_display_work_q(), &apply_work);
}

K_WORK_DEFINE(apply_hop_work, apply_hop_cb);

static void queue_apply(void) {
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &apply_hop_work);
    }
}

// Guard: once more 100 ms after a wake-up, in case a wake came from another thread.
static void apply_later_cb(struct k_work *work) { queue_apply(); }

K_WORK_DELAYABLE_DEFINE(apply_later_work, apply_later_cb);

// Start: ZMK reads settings before it starts the display, and lights the
// screen when it does. Wait for that, then apply (gives up after 10 s).
#define START_CHECK_MS 50
#define START_CHECKS_MAX 200

static int start_checks;

static void start_cb(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(start_work, start_cb);

static void start_cb(struct k_work *work) {
    if (zmk_display_is_initialized()) {
        queue_apply();
    } else if (++start_checks < START_CHECKS_MAX) {
        k_work_schedule(&start_work, K_MSEC(START_CHECK_MS));
    } else {
        LOG_WRN("Display not ready, screen toggle start skipped");
    }
}

static int screen_toggle_init(void) {
    k_work_schedule(&start_work, K_MSEC(START_CHECK_MS));
    return 0;
}

SYS_INIT(screen_toggle_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int screen_toggle_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev != NULL && ev->state == ZMK_ACTIVITY_ACTIVE) {
        queue_apply();
        k_work_reschedule(&apply_later_work, K_MSEC(100));
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(screen_toggle, screen_toggle_listener);
ZMK_SUBSCRIPTION(screen_toggle, zmk_activity_state_changed);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    bool off = !atomic_get(&screen_off);

    atomic_set(&screen_off, off);
    LOG_INF("Screen %s", off ? "off" : "on");
    queue_apply();
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_screen_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define SCREEN_TOGGLE_INST(n)                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_screen_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCREEN_TOGGLE_INST)
