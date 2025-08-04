#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/byteorder.h>

#include <caf/events/button_event.h>
#include "test_mode_event.h"

#define MODULE test_mode
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE	1024
#define THREAD_PRIORITY		K_PRIO_PREEMPT(0)

static K_SEM_DEFINE(sem, 1, 1);
static K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread;

struct k_spinlock lock;
static volatile bool sw = false;
static volatile uint32_t count = 0;
static bool test_mode_on = false;

#include <caf/key_id.h>

static const uint16_t test_key_trans_table[] = {
#if (CONFIG_CY25KB_DRYCELL || CONFIG_CY25KB_RECHARGEABLE)
	KEY_ID(0x01, 0x07),/*mode sw*/                          KEY_ID(0x0C, 0x00),//F13
	KEY_ID(0x08, 0x08),/*Collabs Video / Caculator */       KEY_ID(0x0C, 0x01),//F14
	KEY_ID(0x02, 0x07),/*Collabs Sharing Screen /CE*/       KEY_ID(0x0C, 0x02),//F15
    KEY_ID(0x02, 0x0A),/*Collabs Chat / /+/-*/              KEY_ID(0x0C, 0x03),//F16
    KEY_ID(0x0B, 0x08),/*collabs mic mute / mode switch*/   KEY_ID(0x0C, 0x04),//F17
    KEY_ID(0x07, 0x0B),/*FN*/                               KEY_ID(0x0C, 0x05),//F18
#elif (CONFIG_CY25MS_DRYCELL)
#endif
};

static void PER_thread_fn(void)
{
	while(1) {
        LOG_DBG("%s", __func__);
        k_sleep(K_MSEC(8));
        struct test_mode_event *ev = new_test_mode_event(sizeof(count));
        ev->test_id = test_id_PER_tx;
        memcpy((uint8_t*)ev->dyndata.data, (uint8_t*)&count, sizeof(count));
        LOG_DBG("index:%u", (uint32_t)*ev->dyndata.data);
        APP_EVENT_SUBMIT(ev);
        if (count == 22500) {
            sw = false;
            count = 0;
            k_thread_suspend(&thread);
        } else {
            count ++;
        }

	}
}

static bool handle_button_event(const struct button_event *event)
{
    static bool open_thread_sw = false;
    if (!test_mode_on)
        return false;
    LOG_INF("%s id=0x%04x", __func__, event->key_id);
    if (event->key_id == 0xFFAA) {
        if (event->pressed == true) {
            if (!sw) {
                LOG_INF("thread on");
                sw = true;
                if (open_thread_sw)
                    k_thread_resume(&thread);
                else {
                    open_thread_sw = true;
                    k_thread_start(&thread);
                }
            } else {
                LOG_INF("thread off");
                sw = false;
                k_thread_suspend(&thread);
            }
        }
        return true;
    }

    for (int i = 0; i < ARRAY_SIZE(test_key_trans_table); i += 2) {
        if (event->key_id == test_key_trans_table[i]) {
            struct button_event *new_event = new_button_event();
			new_event->pressed = event->pressed;
			new_event->key_id = test_key_trans_table[i + 1];
			APP_EVENT_SUBMIT(new_event);
            return true;
        }
    }

	return false;
}


static bool app_event_handler(const struct app_event_header *aeh)
{

	if (is_module_state_event(aeh)) {
		const struct module_state_event *event =
			cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {

			k_thread_create(&thread, thread_stack,
					THREAD_STACK_SIZE,
					(k_thread_entry_t)PER_thread_fn,
					NULL, NULL, NULL,
					THREAD_PRIORITY, 0, K_FOREVER);
			k_thread_name_set(&thread, MODULE_NAME "_thread");
            module_set_state(MODULE_STATE_READY);

			return false;
		}

		return false;
	}

    if (IS_ENABLED(CONFIG_CAF_BUTTON_EVENTS) &&
        is_button_event(aeh)) {
            return handle_button_event(cast_button_event(aeh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, button_event);
//APP_EVENT_SUBSCRIBE(MODULE, hid_report_sent_event);
