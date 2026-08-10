/*
 * Pause Zephyr ls0xx serial-VCOM thread while the panel is blanked (ZMK idle).
 * Driver has no blanking hook for serial VCOM; suspend/resume by thread name.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE) && IS_ENABLED(CONFIG_THREAD_MONITOR) &&            \
	IS_ENABLED(CONFIG_THREAD_NAME)

#define LS0XX_VCOM_THREAD_NAME "ls0xx_vcom"

static struct k_thread *vcom_thread;
static bool vcom_paused;

static void find_vcom_cb(const struct k_thread *thread, void *user_data)
{
	ARG_UNUSED(user_data);

	const char *name = k_thread_name_get((k_tid_t)thread);

	if (name != NULL && strcmp(name, LS0XX_VCOM_THREAD_NAME) == 0) {
		vcom_thread = (struct k_thread *)thread;
	}
}

static void vcom_ensure_thread(void)
{
	if (vcom_thread != NULL) {
		return;
	}

	k_thread_foreach(find_vcom_cb, NULL);
}

static void vcom_set_paused(bool pause)
{
	vcom_ensure_thread();
	if (vcom_thread == NULL) {
		LOG_WRN("ls0xx_vcom thread not found — VCOM idle pause skipped");
		return;
	}

	if (pause == vcom_paused) {
		return;
	}

	if (pause) {
		k_thread_suspend(vcom_thread);
		LOG_INF("Serial VCOM paused (display idle)");
	} else {
		k_thread_resume(vcom_thread);
		LOG_INF("Serial VCOM resumed");
	}

	vcom_paused = pause;
}

static int vcom_idle_listener(const zmk_event_t *eh)
{
	const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

	if (ev == NULL) {
		return ZMK_EV_EVENT_BUBBLE;
	}

	switch (ev->state) {
	case ZMK_ACTIVITY_ACTIVE:
		vcom_set_paused(false);
		break;
	case ZMK_ACTIVITY_IDLE:
	case ZMK_ACTIVITY_SLEEP:
		vcom_set_paused(true);
		break;
	}

	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(nice_view_gem_vcom_idle, vcom_idle_listener);
ZMK_SUBSCRIPTION(nice_view_gem_vcom_idle, zmk_activity_state_changed);

#endif /* blank-on-idle + thread monitor */
