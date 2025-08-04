#include <zephyr/kernel.h>

#if CONFIG_RING_BUFFER
#include <zephyr/sys/ring_buffer.h>
#endif

#include "multi_link_proto_device.h"
#include "multi_link_setting.h"
#include "multi_link_basic.h"
#include "multi_link_handler.h"
#include "multi_link.h"
#include "../events/battery_read_request_event.h"   //Wayne
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)
#include "motion_sensor_config_event.h"
#endif

void multi_link_handle_battery_query(void)
{
    struct battery_read_request_event *event = new_battery_read_request_event();
    APP_EVENT_SUBMIT(event);
}

#ifdef CONFIG_DPM_ENABLE
#include "dpm_event.h"
#endif

#include "../modules/dpm_config.h"
#include <zephyr/logging/log.h>
#define MODULE multi_link_handler
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

//=============== functions ===========================
#if CONFIG_DPM_ENABLE
static bool send_bluetooth_paired_first_host_name(void);
static bool send_bluetooth_paired_second_host_name(void);
static bool send_firmware_version_and_capability(void);
#endif
#if 0 //to do : need modify
static bool send_get_report_response_data(multi_link_custom_data_t * p_packet, uint32_t data_length);
#endif 
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE)
static bool send_sensor_dpi(void);
static bool send_reprot_rate(void);
#endif

#if CONFIG_RING_BUFFER
//================ sem ===================
static K_SEM_DEFINE(queue_sem, 1, 1);
//================ lock ==================
static struct k_spinlock lock;
//================ timer =================
static void queue_timer_expire(struct k_timer *timer)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	k_sem_give(&queue_sem);
	k_spin_unlock(&lock, key);
}
K_TIMER_DEFINE(queue_timer, queue_timer_expire, NULL);

//================ task queue ============

#define RING_BUF_SIZE 256      
#define TASK_ARG_SIZE 16       

RING_BUF_DECLARE(shared_ring_buf, RING_BUF_SIZE);  

void scheduler_thread(void *p1, void *p2, void *p3)
{
    uint8_t buffer[TASK_ARG_SIZE + 1];  
    while (1) {
		k_sem_take(&queue_sem, K_FOREVER);
		LOG_ERR("%s", __func__);
        size_t size = ring_buf_get(&shared_ring_buf, buffer, sizeof(buffer));
        if (size > 0) {
            uint8_t report_id = buffer[0];
            uint8_t *data = &buffer[0];  
            size_t data_size = size;

            switch (report_id) {
            case REPORT_ID_KEYBOARD_KEYS:
                LOG_DBG("Executing keyboard task...");
                send_hid_report_multi_link_keyboard_keys(data, data_size);
                break;

            case REPORT_ID_MOUSE:
                LOG_DBG("Executing mouse task...");
                uint8_t packet_cnt;
                send_hid_report_multi_link_mouse(data, data_size, &packet_cnt);
                break;

#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
			case REPORT_ID_CONSUMER_CTRL:
				send_hid_report_multi_link_consumer_keys(data, size);
				break;
#endif
#if CONFIG_DPM_ENABLE
	        case REPORT_ID_DPM_INPUT:
				send_hid_report_multi_link_dpm_input(data, size);
				break;

#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
	        case REPORT_ID_BATTERY_LEVEL:
				send_multi_link_battery_level(data, size);
				break;
#endif
            case REPORT_ID_FIRST_HOST_NAME:
                send_bluetooth_paired_first_host_name();
                break;

            case REPORT_ID_SECOND_HOST_NAME:
                send_bluetooth_paired_second_host_name();
                break;

            case REPORT_ID_FW_VERSION_CAPABILITY:
                send_firmware_version_and_capability();
                break;
            case REPORT_ID_SENSOR_DPI:
                break;
            case REPORT_ID_REPORT_RATE:
                break;
#endif
            default:
                LOG_ERR("Unknown report ID: %d", report_id);
                break;
            }
			k_timer_start(&queue_timer, K_MSEC(8), K_NO_WAIT); //interval time
        } else {
            // no data
        }
    }
}

K_THREAD_STACK_DEFINE(scheduler_stack, 2048);
struct k_thread scheduler_thread_data;

void my_task_queue_init(void)
{
    // Create the scheduler thread
    k_thread_create(&scheduler_thread_data, scheduler_stack, K_THREAD_STACK_SIZEOF(scheduler_stack),
                    scheduler_thread, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
}

void add_data_to_queue(const uint8_t *data, size_t size)
{
	uint8_t buffer[TASK_ARG_SIZE + 1];
	memcpy(buffer, data, size);

    while (ring_buf_space_get(&shared_ring_buf) < size) {
        uint8_t discard[TASK_ARG_SIZE + 1];
        ring_buf_get(&shared_ring_buf, discard, sizeof(discard));
        LOG_WRN("Ring buffer full. Oldest data discarded.");
    }

    if (ring_buf_put(&shared_ring_buf, buffer, size) != size) {
        LOG_ERR("Failed to enqueue HID event. This should not happen.");
    }

	if(k_timer_remaining_get(&queue_timer) == 0)
	{
		if (multi_link_proto_device_is_send_data_ready()) 
		{
			k_timer_start(&queue_timer, K_NO_WAIT, K_NO_WAIT);
		}
		else
		{
			k_timer_start(&queue_timer, K_MSEC(1), K_NO_WAIT);
		}		
	}	
}

#endif
//=============== k_work_delayable =====================
static struct k_work_delayable send_get_report_response_work;
static void send_get_report_response_handler(struct k_work *work);


//============= k_spinlock ==================
struct k_spinlock multi_link_lock;


//============= thread ======================
#define THREAD_STACK_SIZE	2048
#define THREAD_PRIORITY		K_PRIO_PREEMPT(0)
static K_SEM_DEFINE(sem, 1, 1);
static K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread;

static void multi_link_handler_thread_fn(void);

static void thread_init(void)
{
    k_thread_create(&thread, thread_stack,
					THREAD_STACK_SIZE,
					(k_thread_entry_t)multi_link_handler_thread_fn,
					NULL, NULL, NULL,
					THREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&thread, "multi_link_handler_thread");
}

//============= k_timer =====================
//send keep connection
static volatile bool             send_keep_connection_timer_running = false;
static void send_keep_connection_timer_handler(struct k_timer *timer)
{
	k_sem_give(&sem);
}
K_TIMER_DEFINE(send_keep_connection_timer, send_keep_connection_timer_handler, NULL);


void send_keep_connection_timer_start(int32_t ms) //uint64_t
{
    if (send_keep_connection_timer_running == false)
    {
		send_keep_connection_timer_running = true;
		k_timer_start(&send_keep_connection_timer, K_MSEC(ms), K_MSEC(ms));
    }	
}
void send_keep_connection_timer_stop(void)
{
    if (send_keep_connection_timer_running == true)
    {
		send_keep_connection_timer_running = false;
		k_timer_stop(&send_keep_connection_timer);
    }	
}

//============== variable ===================
/* Add an extra byte for HID report ID. */
static multi_link_setting_vendor_open_get_data_ex_t g_response_data;
#if CONFIG_DPM_ENABLE
//#if CONFIG_EARY_BATTERY_MEAS
//#include "../hw_interface/battery_meas.h"//aw
//#endif
static struct k_work_delayable send_info_step_work;
static void send_info_step_handler(struct k_work *work);
static uint8_t host_0_id = REPORT_ID_FIRST_HOST_NAME;
uint8_t packet_buffer_host_0[MULTI_LINK_PACKET_DATA_LENGTH_MAX * 2] = {" "}; // 2 packet test
uint8_t total_packet_host_0 = (sizeof(packet_buffer_host_0) % MULTI_LINK_PACKET_DATA_LENGTH_MAX) == 0 ? (sizeof(packet_buffer_host_0) / MULTI_LINK_PACKET_DATA_LENGTH_MAX) :  ( (sizeof(packet_buffer_host_0) + MULTI_LINK_PACKET_DATA_LENGTH_MAX) / MULTI_LINK_PACKET_DATA_LENGTH_MAX);
static uint8_t host_1_id = REPORT_ID_SECOND_HOST_NAME;
uint8_t packet_buffer_host_1[MULTI_LINK_PACKET_DATA_LENGTH_MAX * 2] = {" "}; // 2 packet test
uint8_t total_packet_host_1 = (sizeof(packet_buffer_host_1) % MULTI_LINK_PACKET_DATA_LENGTH_MAX) == 0 ? (sizeof(packet_buffer_host_1) / MULTI_LINK_PACKET_DATA_LENGTH_MAX) :  ( (sizeof(packet_buffer_host_1) + MULTI_LINK_PACKET_DATA_LENGTH_MAX) / MULTI_LINK_PACKET_DATA_LENGTH_MAX);

static uint8_t fw_ver_id = REPORT_ID_FW_VERSION_CAPABILITY;



enum multi_link_send_info_step
{
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
	SEND_INFO_STEP_BATTERY_LEVEL,
#endif
	SEND_INFO_STEP_FIRST_HOST_NAME_1,
	SEND_INFO_STEP_FIRST_HOST_NAME_2,
	SEND_INFO_STEP_SECOND_HOST_NAME_1,
	SEND_INFO_STEP_SECOND_HOST_NAME_2,
	SEND_INFO_STEP_FW_VER_CAPABILITY,
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)
	SEND_INFO_STEP_DPI_REPORT,
	SEND_INFO_STEP_REPORT_RATE,
#endif
  SEND_INFO_STEP_DONE
};
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
static uint8_t  send_info_step_index = SEND_INFO_STEP_BATTERY_LEVEL;
// static uint8_t  send_info_step_index = SEND_INFO_STEP_FIRST_HOST_NAME_1;
#else
static uint8_t  send_info_step_index = SEND_INFO_STEP_FIRST_HOST_NAME_1;
#endif
void multi_link_get_host_name(uint8_t index)
{
	char *host_name = get_host_name(index);
    uint8_t name_len = strlen(host_name) + 1;
	if(index == 1)
	{
        memset(packet_buffer_host_0 ,0x20 ,sizeof(packet_buffer_host_0)); //ascii space 0x20
		memcpy(packet_buffer_host_0 ,host_name ,name_len);
	}
	else
	{
        memset(packet_buffer_host_1 ,0x20 ,sizeof(packet_buffer_host_1));//ascii space 0x20
		memcpy(packet_buffer_host_1 ,host_name ,name_len);		
	}
}

//=============== k_work =====================
void multi_link_send_info_step_work_init(void)
{
    LOG_DBG("%s\n", __func__);
	k_work_init_delayable(&send_info_step_work, send_info_step_handler);
}

void multi_link_send_info_step_work_submit(uint64_t ms)
{
    LOG_DBG("%d\n", send_info_step_index);
	k_work_schedule(&send_info_step_work ,K_MSEC(ms));
}

#if CONFIG_DPM_ENABLE
static void send_info_step_handler(struct k_work *work)
{ 
    LOG_ERR("%s ,send_info_step_index = %d\n", __func__, send_info_step_index);

    switch (send_info_step_index)
    {
	

#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
       case SEND_INFO_STEP_BATTERY_LEVEL:
#if 0	
	    battery_level_data[0] = REPORT_ID_BATTERY_LEVEL;
        battery_level_data[1] = g_battery;

#if CONFIG_RING_BUFFER	
	     add_data_to_queue(battery_level_data ,sizeof(battery_level_data));
#else
	     send_multi_link_battery_level(battery_level_data, sizeof(battery_level_data));
#endif
#endif
#endif
       
           multi_link_handle_battery_query(); 
        
         break;
       
       case SEND_INFO_STEP_FIRST_HOST_NAME_1:
	      multi_link_get_host_name(1);
#if CONFIG_RING_BUFFER	
	     add_data_to_queue(&host_0_id ,sizeof(host_0_id));
#else
          send_bluetooth_paired_first_host_name();
#endif
         break;
 
       case SEND_INFO_STEP_FIRST_HOST_NAME_2:
#if CONFIG_RING_BUFFER	
	     add_data_to_queue(&host_0_id ,sizeof(host_0_id));
#else
          send_bluetooth_paired_first_host_name();
#endif
         break;

       case SEND_INFO_STEP_SECOND_HOST_NAME_1:
	      multi_link_get_host_name(2);
#if CONFIG_RING_BUFFER	
	     add_data_to_queue(&host_1_id ,sizeof(host_1_id));
#else
          send_bluetooth_paired_second_host_name();
#endif
         break;
 
       case SEND_INFO_STEP_SECOND_HOST_NAME_2:
#if CONFIG_RING_BUFFER	
	     add_data_to_queue(&host_1_id ,sizeof(host_1_id));
#else
          send_bluetooth_paired_second_host_name();
#endif
         break;
       case SEND_INFO_STEP_FW_VER_CAPABILITY:
#if CONFIG_RING_BUFFER	
	     add_data_to_queue(&fw_ver_id ,sizeof(fw_ver_id));
#else
          send_firmware_version_and_capability();
#endif
         break;
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)
       case SEND_INFO_STEP_DPI_REPORT:
         send_sensor_dpi();      
         break;
     
       case SEND_INFO_STEP_REPORT_RATE:
         send_reprot_rate();
         break;  
#endif
    }
    send_info_step_index++;
    if(send_info_step_index < SEND_INFO_STEP_DONE)
    {
        multi_link_send_info_step_work_submit(100);
    }
    else
    {
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
        send_info_step_index = SEND_INFO_STEP_BATTERY_LEVEL;
#else
        send_info_step_index = SEND_INFO_STEP_FIRST_HOST_NAME_1;
#endif
    }
}
#endif
#endif
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS) || (CONFIG_CY25KB_RECHARGEABLE) 

bool send_multi_link_battery_level(const uint8_t *data, size_t size)
{
	LOG_DBG("%s", __func__);

    if (multi_link_proto_device_is_send_data_ready()) 
	{
    	multi_link_battery_data_ex_t multi_link_battery_data_ex;
    	memset(&multi_link_battery_data_ex, 0x00, sizeof(multi_link_battery_data_ex_t));
    	multi_link_battery_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL;
    	multi_link_battery_data_ex.level =  data[1];   
       
#if  (CONFIG_CY25KB_RECHARGEABLE) 
        if(data[2])
        {
            multi_link_battery_data_ex.b0 = 1;
            multi_link_battery_data_ex.b1 = 1;
            multi_link_battery_data_ex.b2 = 0;
        }
        else{
            multi_link_battery_data_ex.b0 = 0;
            multi_link_battery_data_ex.b1 = 0;
            multi_link_battery_data_ex.b2 = 0;

        }
            multi_link_battery_data_ex.b3 = 1;
            multi_link_battery_data_ex.b4 = 1;
            multi_link_battery_data_ex.b5 = 1;
            multi_link_battery_data_ex.b6 = 1;
            multi_link_battery_data_ex.b7 = 0;
            multi_link_battery_data_ex.charge_cycles_counter = 0;
            multi_link_battery_data_ex.loww_batt_counter = 0;
            multi_link_battery_data_ex.mAH =75;
            multi_link_battery_data_ex.multiplier= 2;
          
      

#endif
    	int	send_status = multi_link_proto_device_send_data((uint8_t *)&multi_link_battery_data_ex, sizeof(multi_link_battery_data_ex_t));
    	if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING) 
    	{
      		return true;
    	}
	}

	return false;
}
#endif

bool send_hid_report_multi_link_keyboard_keys(const uint8_t *data, size_t size)
{
	LOG_DBG("%s", __func__);
	uint8_t report_id = data[0];

	__ASSERT_NO_MSG(report_id == REPORT_ID_KEYBOARD_KEYS);

    if (multi_link_proto_device_is_send_data_ready()) 
	{
    	multi_link_kbd_general_key_data_0_ext_t multi_link_kbd_data;
		memset(&multi_link_kbd_data, 0x0, sizeof(multi_link_kbd_data));
    	memcpy((uint8_t*)&multi_link_kbd_data, data, sizeof(multi_link_kbd_general_key_data_0_ext_t));
    	multi_link_kbd_data.type = MULTI_LINK_DEVICE_DATA_TYPE_KBD_GENERAL_KEY;
    	int send_status = multi_link_proto_device_send_data((uint8_t*)&multi_link_kbd_data, sizeof(multi_link_kbd_data));    
    	if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING) 
    	{
      		return true;
    	}
	}
	return false;
}

#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
bool send_hid_report_multi_link_consumer_keys(const uint8_t *data, size_t size)
{
	LOG_DBG("%s", __func__);
	uint8_t report_id = data[0];

	__ASSERT_NO_MSG(report_id == REPORT_ID_CONSUMER_CTRL);

    if (multi_link_proto_device_is_send_data_ready()) 
	{
		multi_link_kbd_consumer_key_data_ext_t consumer_data;
		memset(&consumer_data, 0x0, sizeof(consumer_data));
		consumer_data.key1 = data[1];
		consumer_data.key2 = data[2];
		consumer_data.type = MULTI_LINK_DEVICE_DATA_TYPE_KBD_CONSUMER_KEY;
		int send_status = multi_link_proto_device_send_data((uint8_t*)&consumer_data,
															sizeof(consumer_data));
		if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING) 
		{
			return true;
		}
	}
	return false;
}
#endif
#if CONFIG_DPM_ENABLE
bool send_hid_report_multi_link_dpm_input(const uint8_t *data, size_t size)
{
	LOG_DBG("%s", __func__);
	uint8_t report_id = data[0];

	__ASSERT_NO_MSG(report_id == REPORT_ID_DPM_INPUT);

    if (multi_link_proto_device_is_send_data_ready()) 
	{
		multi_link_setting_collab_keys_button_get_data_ex_t collab_data;
		memset(&collab_data, 0x0, sizeof(collab_data));
		collab_data.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
		collab_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_BUTTONS_GET;
		memcpy((uint8_t*)(&collab_data) + 2 ,&data[2] ,sizeof(data[2]));
        int send_status = multi_link_proto_device_send_data((uint8_t*)&collab_data, sizeof(collab_data));
    	if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING) 
    	{
      		return true;
    	}	
	}
	return false;
}
#endif
bool send_hid_report_multi_link_mouse(const uint8_t *data, size_t size, uint8_t *packet_cnt)
{
	LOG_DBG("%s", __func__);
	uint8_t report_id = data[0];

	__ASSERT_NO_MSG(report_id == REPORT_ID_MOUSE);

    //only one packet to send. reserve this if it needs to send more packets in the future 
	*packet_cnt = 1;

    if (multi_link_proto_device_is_send_data_ready()) 
	{
#if GPIO_TEST
		gpio_pin_set(gpio_devs[1], 8 ,0);
#endif
		/* Mouse report uses 5 bytes:
 	 	 *  8 bits - pressed buttons bitmask
 	 	 *  8 bits - wheel rotation
 	 	 * 12 bits - x movement
 	 	 * 12 bits - y movement
 		 */
    	multi_link_mouse_data_ex_t multi_link_mouse_data;
		memset(&multi_link_mouse_data, 0x0, sizeof(multi_link_mouse_data));
    	multi_link_mouse_data.button = data[1];;
    	multi_link_mouse_data.scroll = data[2];;
    	multi_link_mouse_data.xbit = data[3];
    	multi_link_mouse_data.xybit = data[4];
    	multi_link_mouse_data.ybit = data[5];
		multi_link_mouse_data.type = MULTI_LINK_DEVICE_DATA_TYPE_MOUSE;

		#if CONFIG_DESKTOP_TILT_BUTTONS_ENABLE
		multi_link_mouse_data.pan = data[6]; //pan
		#endif
    	int send_status = multi_link_proto_device_send_data((uint8_t *)&multi_link_mouse_data, sizeof(multi_link_mouse_data_ex_t));       
    	if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING) 
    	{
      		return true;
    	}
	}
	return false;
}
#if CONFIG_DPM_ENABLE
static bool send_bluetooth_paired_first_host_name(void)
{
    int send_status; 
    if (multi_link_proto_device_is_send_data_ready())
    {  
        if(g_packet_id_host_0 != total_packet_host_0)
        {
            LOG_DBG("%s\n", __func__);
            multi_link_multi_packet_hostname_data_ex_t multi_link_multi_packet_hostname_data_ex;
            memset(&multi_link_multi_packet_hostname_data_ex, 0x00, sizeof(multi_link_multi_packet_hostname_data_ex_t));
            multi_link_multi_packet_hostname_data_ex.header.type = MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_0;
            multi_link_multi_packet_hostname_data_ex.header.total_packet = total_packet_host_0;
            multi_link_multi_packet_hostname_data_ex.header.packet_id = g_packet_id_host_0;
            multi_link_multi_packet_hostname_data_ex.header.data_length = MULTI_LINK_PACKET_DATA_LENGTH_MAX;
				

            if( ((g_packet_id_host_0 + 1) * MULTI_LINK_PACKET_DATA_LENGTH_MAX) > sizeof(packet_buffer_host_0) )
            {
                multi_link_multi_packet_hostname_data_ex.header.data_length = sizeof(packet_buffer_host_0) - ((g_packet_id_host_0 ) * MULTI_LINK_PACKET_DATA_LENGTH_MAX);
            }
	
            memcpy(multi_link_multi_packet_hostname_data_ex.data,&packet_buffer_host_0[g_packet_id_host_0 * MULTI_LINK_PACKET_DATA_LENGTH_MAX], multi_link_multi_packet_hostname_data_ex.header.data_length);
            send_status = multi_link_proto_device_send_data((uint8_t *)&multi_link_multi_packet_hostname_data_ex, sizeof(multi_link_multi_packet_hostname_data_ex_t));
            if(send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
            {								
                g_packet_id_host_0++;
           
                if( g_packet_id_host_0 == total_packet_host_0)
                {
                    g_packet_id_host_0 = 0;
                }  
                return true;
            }
            else
            {
                g_packet_id_host_0 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;
                LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_0");
            }
        }
        else
        {
            return true;
        }  
    }

  return false;
}


static bool send_bluetooth_paired_second_host_name(void)
{
    int send_status; 
    if (multi_link_proto_device_is_send_data_ready())
    {  
        if(g_packet_id_host_1 != total_packet_host_1)
        {
            LOG_DBG("%s\n", __func__);
            multi_link_multi_packet_hostname_data_ex_t multi_link_multi_packet_hostname_data_ex;
            memset(&multi_link_multi_packet_hostname_data_ex, 0x00, sizeof(multi_link_multi_packet_hostname_data_ex_t));
            multi_link_multi_packet_hostname_data_ex.header.type = MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_1;
            multi_link_multi_packet_hostname_data_ex.header.total_packet = total_packet_host_1;
            multi_link_multi_packet_hostname_data_ex.header.packet_id = g_packet_id_host_1;
            multi_link_multi_packet_hostname_data_ex.header.data_length = MULTI_LINK_PACKET_DATA_LENGTH_MAX;
				
            if( ((g_packet_id_host_1 + 1) * MULTI_LINK_PACKET_DATA_LENGTH_MAX) > sizeof(packet_buffer_host_1))
            {
                multi_link_multi_packet_hostname_data_ex.header.data_length = sizeof(packet_buffer_host_1) - ((g_packet_id_host_1 ) * MULTI_LINK_PACKET_DATA_LENGTH_MAX);   
            }
					
            memcpy(multi_link_multi_packet_hostname_data_ex.data,&packet_buffer_host_1[g_packet_id_host_1 * MULTI_LINK_PACKET_DATA_LENGTH_MAX], multi_link_multi_packet_hostname_data_ex.header.data_length);
            send_status = multi_link_proto_device_send_data((uint8_t *)&multi_link_multi_packet_hostname_data_ex, sizeof(multi_link_multi_packet_hostname_data_ex_t));
            if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
            {								
                g_packet_id_host_1++;
           
                if( g_packet_id_host_1 == total_packet_host_1)
                {
                    g_packet_id_host_1 = 0;
                }  
                return true;
            }
            else
            {
                g_packet_id_host_1 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;
                LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_1");
            }
        }
        else
        {
            return true;
        }  
    }

  return false;
}

static bool send_firmware_version_and_capability(void)
{
    int send_status;
    if (multi_link_proto_device_is_send_data_ready())
	{
		uint16_t firmware_version = multi_link_device_get_device_firmware_version();
		multi_link_firmware_version_capability_data_ex_t multi_link_firmware_version_capability_data_ex;
		memset(&multi_link_firmware_version_capability_data_ex, 0x00, sizeof(multi_link_firmware_version_capability_data_ex_t));
		multi_link_firmware_version_capability_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_FW_VERSION_CAPABILITY;
		multi_link_firmware_version_capability_data_ex.version = firmware_version; // For testing changing version
		multi_link_firmware_version_capability_data_ex.capability = multi_link_device_get_device_capability();
		send_status = multi_link_proto_device_send_data((uint8_t *)&multi_link_firmware_version_capability_data_ex, sizeof(multi_link_firmware_version_capability_data_ex_t));
		if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
        {
			return true;
        }
		else
		{
			LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_DATA_TYPE_FW_VERSION");
		}
	}
    return false;  
}
#endif

#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)

static bool send_sensor_dpi(void)
{
	int send_status;  
  if (multi_link_proto_device_is_send_data_ready())
  { 
	multi_link_setting_dpi_value_get_data_ex_t multi_link_setting_dpi_value_get_data_ex;
	memset(&multi_link_setting_dpi_value_get_data_ex, 0x00, sizeof(multi_link_setting_dpi_value_get_data_ex_t));
	multi_link_setting_dpi_value_get_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
	multi_link_setting_dpi_value_get_data_ex.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET;				
	multi_link_setting_dpi_value_get_data_ex.current_dpi_value = g_current_dpi_value; 
	multi_link_setting_dpi_value_get_data_ex.min_dpi_value =  600;
	multi_link_setting_dpi_value_get_data_ex.max_dpi_value = 4020;
	multi_link_setting_dpi_value_get_data_ex.stepping_delta =  30;

    send_status =  multi_link_proto_device_send_data((uint8_t *)&multi_link_setting_dpi_value_get_data_ex, sizeof(multi_link_setting_dpi_value_get_data_ex_t)); 
	if(send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
	{
		return true;	 
	}
	else
	{
		LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET");
	}
  }
  return false;
}

static bool send_reprot_rate(void)
{
    int send_status;  
  
    multi_link_setting_report_rate_get_data_ex_t multi_link_setting_report_rate_get_data_ex;
	memset(&multi_link_setting_report_rate_get_data_ex, 0x00, sizeof(multi_link_setting_report_rate_get_data_ex_t));
	multi_link_setting_report_rate_get_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
	multi_link_setting_report_rate_get_data_ex.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_GET;				
	multi_link_setting_report_rate_get_data_ex.current_report_rate_hz = *get_report_rate_current(); 
		 
    multi_link_setting_report_rate_get_data_ex.supported_report_rates_hz[0] = *get_report_rate_support();
	multi_link_setting_report_rate_get_data_ex.supported_report_rates_hz[1] = *(get_report_rate_support() + 1);
	multi_link_setting_report_rate_get_data_ex.supported_report_rates_hz[2] = *(get_report_rate_support() + 2);
    multi_link_setting_report_rate_get_data_ex.supported_report_rates_hz[3] = *(get_report_rate_support() + 3);

    send_status =  multi_link_proto_device_send_data((uint8_t *)&multi_link_setting_report_rate_get_data_ex, sizeof(multi_link_setting_report_rate_get_data_ex_t)); 
	if(send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
	{
		return true;	 
	}
	else
	{
		LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_GET");
	}
    return false;
}
#endif

#if 0 //to do : need modify
static bool send_get_report_response_data(multi_link_custom_data_t * p_packet, uint32_t data_length)
{
    int send_status;
    if (multi_link_proto_device_is_send_data_ready())
    {
        multi_link_setting_vendor_open_get_data_ex_t response_data;
        memcpy(&response_data ,p_packet->data, data_length);
        send_status = multi_link_proto_device_send_data((uint8_t *)&response_data, sizeof(response_data));
        if( send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE || send_status == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING)
        {
            LOG_HEXDUMP_INF((uint8_t*)&response_data, data_length, "send_get_report_response_data :");
            return true;
        }
        else
        {
            LOG_ERR("multi_link_proto_device_send_data failed for send_get_report_response_data");
        }
    }
    return false;  
}
#endif

static void send_keep_connection(void)
{
    if(g_current_ping_duration > 0)
    {
        LOG_INF("Tick = %d", (g_current_ping_duration/g_current_ping_interval) );
        multi_link_setting_keep_connection_get_data_ex_t multi_link_setting_keep_connection_get_data_ex;
        memset(&multi_link_setting_keep_connection_get_data_ex, 0x00, sizeof(multi_link_setting_keep_connection_get_data_ex_t));
        multi_link_setting_keep_connection_get_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
        multi_link_setting_keep_connection_get_data_ex.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_GET;				
        multi_link_setting_keep_connection_get_data_ex.remaining_tick_count = (g_current_ping_duration/g_current_ping_interval);

        g_current_ping_duration -= g_current_ping_interval;						
        g_last_ping_sent_time = multi_link_device_get_uptime_ms();
        if(g_current_ping_duration  == 0)
        {
            send_keep_connection_timer_stop();    
        }
        
        if(multi_link_proto_device_send_data((uint8_t *)&multi_link_setting_keep_connection_get_data_ex, sizeof(multi_link_setting_keep_connection_get_data_ex_t)) != MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE)
        {
           g_current_ping_duration = 0;
           LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_GET");
        }
 
    }
    else
    {
        send_keep_connection_timer_stop();
    }      
}  

 

bool multi_link_device_set_settings(uint8_t settings_type, uint8_t* data, uint8_t data_length)
{

	//
	// Note this callback can call mutiple times for same value so try to optimize by checking previous value
	// WARNING: Must return from this callback before doing any operation so dongle can get conformation
	//

	//
	// In this callback only allow to call multi_link_proto_device_request_dynamic_key. Do not call an other APIs
	//

	switch(settings_type)
	{
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_SET:
		{

			multi_link_setting_dpi_value_set_value_ex_t* multi_link_setting_dpi_value_set_value_ex = (multi_link_setting_dpi_value_set_value_ex_t*) data;
			if(data_length == sizeof(multi_link_setting_dpi_value_set_value_ex_t) && multi_link_setting_dpi_value_set_value_ex->dpi_value != g_current_dpi_value)
			{
				
				g_current_dpi_value = multi_link_setting_dpi_value_set_value_ex->dpi_value;
				send_set_dpi_event(g_current_dpi_value);
				LOG_INF("Setting new DPI value : %d", g_current_dpi_value);
			}
			return true;
		}

    	case MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_SET:
    	{   
      		multi_link_setting_report_rate_set_value_ex_t* multi_link_setting_report_rate_set_value_ex = (multi_link_setting_report_rate_set_value_ex_t*) data;
      		if( data_length == sizeof(multi_link_setting_report_rate_set_value_ex_t) && multi_link_setting_report_rate_set_value_ex->report_rate_hz != *get_report_rate_current())
      		{
				send_set_report_rate_event(multi_link_setting_report_rate_set_value_ex->report_rate_hz);
      		}
      		return true;
    	}
#endif
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_LED_STATUS_SET:
		{
			multi_link_setting_led_status_set_value_ex_t *multi_link_setting_led_status_set_value_ex = (multi_link_setting_led_status_set_value_ex_t*) data;
			if(data_length == sizeof(multi_link_setting_led_status_set_value_ex_t) && data[0] != g_current_led_status)			
			{
				g_current_led_status = data[0];
                multilink_send_kbd_leds_report(g_current_led_status);

				LOG_INF("Setting new LED status. num_lock : %d, caps_lock : %d, scroll_lock : %d", multi_link_setting_led_status_set_value_ex->num_lock, multi_link_setting_led_status_set_value_ex->caps_lock, multi_link_setting_led_status_set_value_ex->scroll_lock);
			}
			return true;
		}
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_ENTER_OTA_MODE_SET:
		{
			//
			// Device should start OTA flow based on OTA protocal
			// Also device must set provided value in Gazell 
			// WARNING: do not start OTA here should start in main loop. This callback must return before OTA start so dongle get conformation
			//
			if(data_length == sizeof(multi_link_setting_ota_enter_set_value_ex_t))			
			{
				multi_link_setting_ota_enter_set_value_ex_t *multi_link_setting_ota_enter_set_value_ex = (multi_link_setting_ota_enter_set_value_ex_t*) data;
				LOG_INF("Dongle requested to enter OTA mode, timeslot_period : %d, channel_selection_policy : %d", multi_link_setting_ota_enter_set_value_ex->timeslot_period, multi_link_setting_ota_enter_set_value_ex->channel_selection_policy);
			}
            extern void OTA_start(void);
            extern void OTA_init(void);
            OTA_init();
            OTA_start();
			return true; 
		}
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_SET:
		{
			//
			// Device should keep connection and ping based on value
			//
			if(data_length == sizeof(multi_link_setting_keep_connection_set_value_ex_t))			
			{
				int32_t ping_interval;
				int32_t ping_duration;

				multi_link_setting_keep_connection_set_value_ex_t *multi_link_setting_keep_connection_set_value_ex = (multi_link_setting_keep_connection_set_value_ex_t*) data;				

				ping_interval = multi_link_setting_keep_connection_set_value_ex->interval;
				ping_duration = (multi_link_setting_keep_connection_set_value_ex->duration * 1000);

				if(ping_interval < 8)
					ping_interval = 8;

				if(multi_link_setting_keep_connection_set_value_ex->multiplier)
					ping_interval *= 8;

				if(g_current_ping_interval != ping_interval || g_current_ping_duration != ping_duration)
				{
					g_current_ping_interval = ping_interval;
					g_current_ping_duration = ping_duration;

					g_last_ping_sent_time = multi_link_device_get_uptime_ms();

                    send_keep_connection_timer_start(g_current_ping_interval);

					LOG_INF("Dongle requested to keep connection. interval : %d, duration : %d, multiplier : %d", multi_link_setting_keep_connection_set_value_ex->interval, multi_link_setting_keep_connection_set_value_ex->duration, multi_link_setting_keep_connection_set_value_ex->multiplier);
					LOG_INF("Final keep connection. interval : %d, duration : %d", g_current_ping_interval, g_current_ping_duration);
				}
			}
			return true;
		}
#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT	
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_SET:
		{
			multi_link_setting_collab_keys_setting_set_value_ex_t *multi_link_setting_collab_keys_setting_set_value_ex = (multi_link_setting_collab_keys_setting_set_value_ex_t*) data;				

			LOG_INF("Dongle requested to collab keys. enable_video : %d, enable_share : %d, enable_chat : %d, enable_mic_mute : %d, chat_led_blink_effect_enable : %d, tap_max_duration : %d", multi_link_setting_collab_keys_setting_set_value_ex->enable_video, multi_link_setting_collab_keys_setting_set_value_ex->enable_share, multi_link_setting_collab_keys_setting_set_value_ex->enable_chat, multi_link_setting_collab_keys_setting_set_value_ex->enable_mic_mute, multi_link_setting_collab_keys_setting_set_value_ex->chat_led_blink_effect_enable, multi_link_setting_collab_keys_setting_set_value_ex->tap_max_duration);
			
			set_collabs_keys_status((uint8_t*)multi_link_setting_collab_keys_setting_set_value_ex);
			
			return true;
		}
#endif
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_0:
		{
			LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_0 :");
			return true;
		}
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_1:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_1 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_2:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_2 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_3:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_3 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_4:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_4 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_5:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_5 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_6:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_6 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_7:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_7 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_8:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_8 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_9:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_9 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_10:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_10 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_11:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_11 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_12:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_12 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_13:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_13 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_14:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_14 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_15:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_15 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_16:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_16 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_17:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_17 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_18:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_18 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_19:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_19 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_20:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_20 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_21:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_21 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_22:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_22 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_23:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_23 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_24:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_24 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_25:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_25 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_26:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_26 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_27:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_27 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_28:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_28 :");
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_29:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_29 :");
            return true;
        }
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_0:
		{
			LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_0 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_0;
            multi_link_send_get_report_response_work_submit(100);
			return true;
		}
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_1:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_1 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_1;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_2:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_2 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_2;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_3:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_3 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_3;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_4:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_4 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_4;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_5:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_5 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_5;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_6:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_6 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_6;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_7:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_7 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_7;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_8:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_8 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_8;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_9:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_9 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_9;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_10:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_10 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_10;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_11:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_11 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_11;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_12:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_12 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_12;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_13:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_13 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_13;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_14:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_14 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_14;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_15:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_15 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_15;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_16:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_16 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_16;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_17:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_17 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_17;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_18:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_18 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_18;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_19:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_19 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_19;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_20:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_20 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_20;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_21:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_21 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_21;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_22:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_22 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_22;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_23:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_23 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_23;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_24:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_24 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_24;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_25:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_25 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_25;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_26:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_26 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_26;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_27:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_27 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_27;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_28:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_28 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_28;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
        case MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_29:
        {
            LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_29 :");
            g_response_data.settings_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_29;
            multi_link_send_get_report_response_work_submit(100);
            return true;
        }
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET:
		{
			//
			// Demo to send DPI when dongle ask. Same log can be implemented for any GET settings 
			// Device must support for all related GET settings that defined in enum multi_link_device_settings_type
			//
			LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET :");
			//g_current_dpi_value = MULTI_LINK_SETTING_DATA_DPI_VALUE_UNKNOW;
			return true;
		}
		case MULTI_LINK_DEVICE_SETTINGS_TYPE_UPDATE_CHANNEL_TABLE_SET:
		{
		    uint8_t current_channels[NRF_GZLL_CONST_MAX_CHANNEL_TABLE_SIZE];

			LOG_HEXDUMP_INF(data, data_length, "MULTI_LINK_DEVICE_SETTINGS_TYPE_UPDATE_CHANNEL_TABLE_SET :");

			//
			// NOTE: Device must process this setting
			//
			bool is_update_required = false;

			uint32_t channel_table_size = nrf_gzll_get_channel_table_size();

			if(channel_table_size != data_length)
			{
				is_update_required = true;
			}

			if(!is_update_required)
			{
				
				if(nrf_gzll_get_channel_table(current_channels,&channel_table_size))
				{
					for(uint32_t index = 0; index < channel_table_size; index++)
					{
						if(current_channels[index] != data[index])
						{
							is_update_required = true;
							break;
						}
					}
				}
				else
				{
					is_update_required = true;
				}

			}

			LOG_HEXDUMP_INF(current_channels, channel_table_size, "Current channel table");

			if(is_update_required)
			{
				LOG_HEXDUMP_INF(data, data_length, "New channel table");

				nrf_gzll_disable();
				while (nrf_gzll_is_enabled())
				{
				}

				nrf_gzll_set_channel_table(data, data_length);

				nrf_gzll_enable();

			}

			return true;
		}
		default:
		{
			LOG_WRN("Unknown setting type : %d",settings_type);
			break;
		}
	}
	
	return false; // returning false, dongle will get setting status fail. 

}


void multi_link_send_get_report_response_work_init(void)
{
  LOG_ERR("%s\n", __func__);
    k_work_init_delayable(&send_get_report_response_work, send_get_report_response_handler);
}

void multi_link_send_get_report_response_work_submit(uint64_t ms)
{
    k_work_schedule(&send_get_report_response_work ,K_MSEC(ms));
}

static void send_get_report_response_handler(struct k_work *work)
{
#if 0 //to do : need modify
    static bool send_status;
    static multi_link_custom_data_t custom_data;
    static uint8_t rolling_code = 0;

    g_response_data.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
    g_response_data.vendor_data[0] = 0xaa;
    g_response_data.vendor_data[1] = 0xbb;
    g_response_data.vendor_data[2] = 0xcc;
    g_response_data.vendor_data[3] = 0xdd;
    g_response_data.vendor_data[4] = 0xee;
    g_response_data.vendor_data[5] = rolling_code ++;
    memcpy(custom_data.data ,&g_response_data ,sizeof(g_response_data));
    custom_data.type = MULTI_LINK_CUSTOM_PACKET_TYPE_GET_REPORT_RESPONSE;

    send_status = send_multi_link_by_type(&custom_data ,sizeof(g_response_data));
#endif
}

static void multi_link_handler_thread_fn(void)
{
    while(true)
    {
        k_sem_take(&sem, K_FOREVER);
        //keep connection
        send_keep_connection();
    }
}

void multi_link_handler_init(void)
{
    thread_init();
#if CONFIG_DPM_ENABLE
	multi_link_send_info_step_work_init();
	multi_link_send_get_report_response_work_init();
#endif
}
