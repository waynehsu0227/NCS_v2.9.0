#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <caf/events/power_manager_event.h>

#define MODULE uart_handler
#include <caf/events/module_state_event.h>

#if CONFIG_RADIO_TEST_ENABLE
#include "radio_test.h"
#endif

#include <stdarg.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

#define UART_BUF_SIZE 40
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX 50000 //us

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(darfon_factory_uart));
static struct k_work_delayable uart_work;

struct uart_data_t {
	void *fifo_reserved;
	uint8_t data[UART_BUF_SIZE];
	uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static bool check_uart_driver_status(void)
{
    const struct device *dev;

    dev = device_get_binding("uart@c6000"); //uart20;

    if (dev) 
	{
        LOG_ERR("uart_driver initialized successfully.\n");
		return true;
    } else 
	{
        LOG_ERR("uart_driver is not initialized.\n");
		return false;
    }
}
//============= thread ======================
#define UART_HANDLER_THREAD_STACK_SIZE	2048
#define UART_HANDLER_HREAD_PRIORITY		K_PRIO_PREEMPT(0)
static K_SEM_DEFINE(uart_handler_sem, 1, 1);
static K_THREAD_STACK_DEFINE(uart_handler_thread_stack, UART_HANDLER_THREAD_STACK_SIZE);
static struct k_thread uart_handler_thread;

static void uart_handler_thread_fn(void);

static void thread_init(void)
{
    k_thread_create(&uart_handler_thread, uart_handler_thread_stack,
					UART_HANDLER_THREAD_STACK_SIZE,
					(k_thread_entry_t)uart_handler_thread_fn,
					NULL, NULL, NULL,
					UART_HANDLER_HREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&uart_handler_thread, "multi_link_handler_thread");
}
//============= k_timer =====================
#define  FACTORY_TIMER_POLLING_INTERVAL_MS        4
static void factory_timer_cb(struct k_timer *timer)
{
	k_sem_give(&uart_handler_sem);
}
K_TIMER_DEFINE(factory_timer, factory_timer_cb, NULL);

void factory_timer_start(uint64_t ms) //uint64_t
{
	k_timer_start(&factory_timer, K_MSEC(ms), K_MSEC(ms));
}

//============= uart printf =====================
static void uart_printf(const char *format, ...)
{
	va_list args;
	int err;
	int pos;
	struct uart_data_t *tx;

	va_start(args, format);

	tx = k_malloc(sizeof(*tx));

	if (tx) {
		pos = vsnprintf(tx->data, sizeof(tx->data), format, args);

		if ((pos < 0) || (pos >= sizeof(tx->data))) {
			k_free(tx);
			LOG_ERR("snprintf returned %d", pos);
			return -ENOMEM;
		}

		tx->len = pos;
	} else {
		return -ENOMEM;
	}

	err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
	if (err) {
		k_free(tx);
		LOG_ERR("Cannot tx fcc_test (err: %d)", err);
		return err;
	}

    va_end(args);
}


//====== darfon factory command =====
typedef struct 
{
	uint8_t cmd_hd;
	void (*command_function)(uint8_t* cmd_dat ,uint8_t length);
}command_list_t;

//=== CMD HD ===
#define DEVICE_TO_HOST_MASK              0x80
#define TEST_MODE_START_CMD_HD           0xaa
#define READ_FW_VERSION_CMD_HD           0x01
#define MOUSE_BASIC_FUNCTION_CMD_HD      0x02
#define BATTERY_LEVEL_CMD_HD             0x03
#define MODE_LED_TEST_CMD_HD             0x04
#define FCC_TEST_CMD_HD                  0x06
#define KEYBOAD_BASIC_FUNCTION_CMD_HD    0x07
#define SENSOR_ID_CMD_HD                 0x08
#define SAADC_ENABLE_CMD_HD              0x0A
#define SAADC_DISABLE_CMD_HD             0x0B
#define BER_TEST_CMD_HD                  0x0C
#define READ_RF_ADDR_CMD_HD              0x0E
#define READ_FLASH_DATA_CMD_HD           0x0F




//=== CMD DAT ===
#define TEST_MODE_START_CMD_DAT   0xbb

//FCC
#define RADIO_TEST_SET_CHANNEL_LOW            0x3A
#define RADIO_TEST_SET_CHANNEL_MID            0x3B
#define RADIO_TEST_SET_CHANNEL_HIGH           0x3C
#define RADIO_TEST_TX_CARRIER                 0x3D
#define RADIO_TEST_RX_CARRIER                 0x3E
#define RADIO_TEST_TX_MODULATED_CARRIER       0x3F
#define RADIO_TEST_TX_CARRIER_SWEEP           0x40 
#define RADIO_TEST_TX_RANG_TEST               0x41
#define RADIO_TEST_LED_TEST                   0x42
#define RADIO_TEST_FACTORY_OUT_SET            0x43
#define RADIO_TEST_SLEEP_MODE                 0x44
#define RADIO_TEST_SEND_FACTORY_CMD           0x45


//=== command function ===
static void test_mode_start(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void read_fw_version(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void mouse_basic_function(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void battery_level(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void mode_led_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void fcc_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void keyboard_basic_function(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void sensor_id(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void saadc_enable(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void saadc_disable(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void ber_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void read_rf_addr(uint8_t *cmd_dat ,uint8_t cmd_dat_length);
static void read_flash_data(uint8_t *cmd_dat ,uint8_t cmd_dat_length);


//=== factory command ===
#define MAX_FACTORY_CMD_NUMBER      13
command_list_t factory_cmd_list[MAX_FACTORY_CMD_NUMBER];

void factory_command_register(uint8_t cmd_index, uint8_t cmd_hd ,void (*func_ptr)(uint8_t * ,uint8_t))
{
	factory_cmd_list[cmd_index].cmd_hd = cmd_hd;
	factory_cmd_list[cmd_index].command_function = func_ptr;
}

void factory_command_init(void)
{
	uint8_t cmd_index = 0;
	factory_command_register(cmd_index++ ,TEST_MODE_START_CMD_HD ,test_mode_start); //1
	factory_command_register(cmd_index++ ,READ_FW_VERSION_CMD_HD ,read_fw_version); //2
	factory_command_register(cmd_index++ ,MOUSE_BASIC_FUNCTION_CMD_HD ,mouse_basic_function); //3
	factory_command_register(cmd_index++ ,BATTERY_LEVEL_CMD_HD ,battery_level); //4
	factory_command_register(cmd_index++ ,MODE_LED_TEST_CMD_HD ,mode_led_test); //5
	factory_command_register(cmd_index++ ,FCC_TEST_CMD_HD ,fcc_test); //6
	factory_command_register(cmd_index++ ,KEYBOAD_BASIC_FUNCTION_CMD_HD ,keyboard_basic_function); //7
	factory_command_register(cmd_index++ ,SENSOR_ID_CMD_HD ,sensor_id); //8
	factory_command_register(cmd_index++ ,SAADC_ENABLE_CMD_HD ,saadc_enable); //9
	factory_command_register(cmd_index++ ,SAADC_DISABLE_CMD_HD ,saadc_disable); //10
	factory_command_register(cmd_index++ ,BER_TEST_CMD_HD ,ber_test); //11
	factory_command_register(cmd_index++ ,READ_RF_ADDR_CMD_HD ,read_rf_addr); //12
	factory_command_register(cmd_index++ ,READ_FLASH_DATA_CMD_HD ,read_flash_data); //13
}

#define HOST_TO_DEVICE_START      0x55
#define DEVICE_TO_HOST_START      0xD5
#define HOST_TO_DEVICE_STOP       0x44
#define DEVICE_TO_HOST_STOP       0xD4

#define PACKET_HEAD_TAIL_LENGHT    6

#define FACTORY_UART_RX_BUF_SIZE   256
typedef enum _parse_stage_t
{
	PS_START = 0,
	PS_LEN,
	PS_CMD_HD,
	PS_CMD_DAT,
	PS_CHKSM_LOW_BYTE,
	PS_CHKSM_HIGH_BYTE,
	PS_STOP
}parse_stage_t ;

parse_stage_t parse_rx_stage = PS_START;
static uint8_t factory_uart_rx_buf[FACTORY_UART_RX_BUF_SIZE];
static uint32_t rx_head;
static uint32_t rx_tail;
static uint8_t cmd_dat[UART_BUF_SIZE];

#define MAX_TIMEOUT_COUNT   3
static uint8_t process_timeout_count;
static void factory_rx_buf_process(void)
{
    uint8_t datax;
    size_t cmd_dat_length = 0;
	uint8_t cmd_hd;
	size_t cmd_dat_index = 0;
	uint8_t chksm_low_byte;
	uint8_t chksm_high_byte;	
	uint16_t chksm_value = 0;
    
	if(parse_rx_stage != PS_START)
	{
		if(process_timeout_count++ > MAX_TIMEOUT_COUNT)
		{
			process_timeout_count = 0;
			parse_rx_stage = PS_START;
		}
	}
	while(rx_tail != rx_head)
	{
		process_timeout_count = 0;
		datax = factory_uart_rx_buf[rx_tail++];
		if(rx_tail >= FACTORY_UART_RX_BUF_SIZE)
		{
			rx_tail = 0;
		}
		switch(parse_rx_stage)
		{
			case PS_START:
			    LOG_DBG("PS_START");
				chksm_value = 0;
				if(datax == HOST_TO_DEVICE_START)
				{
					parse_rx_stage = PS_LEN;
					cmd_dat_index = 0;
					chksm_value += datax;
				}
				break;
			case PS_LEN:
				LOG_DBG("PS_LEN");
			    if(datax > PACKET_HEAD_TAIL_LENGHT)
				{
					cmd_dat_length = datax - PACKET_HEAD_TAIL_LENGHT;
					parse_rx_stage = PS_CMD_HD;
					chksm_value += datax;
				}
				else
				{
					parse_rx_stage = PS_START;
				}

				break;
			case PS_CMD_HD:
			    LOG_DBG("PS_CMD_HD");
			    cmd_hd = datax;
				parse_rx_stage = PS_CMD_DAT;
				chksm_value += datax;
				break;
			case PS_CMD_DAT:
			    LOG_DBG("PS_CMD_DAT");
			    cmd_dat[cmd_dat_index++] = datax;
				chksm_value += datax;
				if(cmd_dat_index >= cmd_dat_length)
				{
					parse_rx_stage = PS_CHKSM_LOW_BYTE;
				}
				break;
			case PS_CHKSM_LOW_BYTE:
			    LOG_DBG("PS_CHKSM_LOW_BYTE, chksm_value = 0x%04x",chksm_value);
				chksm_low_byte = datax;
				if(chksm_low_byte == (chksm_value & 0xFF))
				{
					parse_rx_stage = PS_CHKSM_HIGH_BYTE;
				}
				else
				{
					parse_rx_stage = PS_START;
				}
				break;
			case PS_CHKSM_HIGH_BYTE:
				LOG_DBG("PS_CHKSM_HIGH_BYTE");
			    chksm_high_byte = datax;
				if(chksm_high_byte == (chksm_value >> 8))
				{
					parse_rx_stage = PS_STOP;
				}
				else
				{
					parse_rx_stage = PS_START;
				}
				break;
			case PS_STOP:
			    LOG_DBG("PS_STOP");
			    if(datax == HOST_TO_DEVICE_STOP)
				{
					for(uint8_t i=0 ;i < MAX_FACTORY_CMD_NUMBER ;i++)
					{
						if(factory_cmd_list[i].cmd_hd == cmd_hd)
						{
							factory_cmd_list[i].command_function(cmd_dat ,cmd_dat_length);
							break;
						}
					}
				}
			    parse_rx_stage = PS_START;
				break;
		}
	}
}

//======= test_mode_start =======
static void test_mode_start(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("test_mode_start\r\n");
}

//======= read fw version =======
static void read_fw_version(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("read_fw_version\r\n");
}

//======= mouse_basic_function =======
static void mouse_basic_function(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("mouse_basic_function\r\n");
}

//======= battery_level =======
static void battery_level(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("battery_level\r\n");
}

//======= mode_led_test =======
static void mode_led_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("mode_led_test\r\n");
}
/* Radio parameter configuration. */
static struct radio_param_config {
	/** Radio transmission pattern. */
	enum transmit_pattern tx_pattern;

	/** Radio mode. Data rate and modulation. */
	nrf_radio_mode_t mode;

	/** Radio output power. */
	int8_t txpower;

	/** Radio start channel (frequency). */
	uint8_t channel_start;

	/** Radio end channel (frequency). */
	uint8_t channel_end;

	/** Radio set channel (frequency). */
	uint8_t channel_current;

	/** Delay time in milliseconds. */
	uint32_t delay_ms;

	/** Duty cycle. */
	uint32_t duty_cycle;

#if CONFIG_FEM
	/* Front-end module (FEM) configuration. */
	struct radio_test_fem fem;
#endif /* CONFIG_FEM */
} config = {
	.tx_pattern = TRANSMIT_PATTERN_RANDOM,
	.mode = NRF_RADIO_MODE_BLE_1MBIT,
	.txpower = 0,
	.channel_start = CONFIG_RADIO_TEST_CHANNEL_LOW,
	.channel_end = CONFIG_RADIO_TEST_CHANNEL_HIGH,
	.delay_ms = 10,
	.duty_cycle = 50,
#if CONFIG_FEM
	.fem.tx_power_control = FEM_USE_DEFAULT_TX_POWER_CONTROL
#endif /* CONFIG_FEM */
};

/* Radio test configuration. */
static struct radio_test_config test_config;

//======= fcc_test =======
static void fcc_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("fcc_test:0x%02X\r\n",cmd_dat[0]);

    switch(cmd_dat[0])
	{
		case RADIO_TEST_SET_CHANNEL_LOW:
			config.channel_current = CONFIG_RADIO_TEST_CHANNEL_LOW;
			break;
		case RADIO_TEST_SET_CHANNEL_MID:
			config.channel_current = CONFIG_RADIO_TEST_CHANNEL_MIDDLE;
			break;
		case RADIO_TEST_SET_CHANNEL_HIGH:
			config.channel_current = CONFIG_RADIO_TEST_CHANNEL_HIGH;
			break;
		case RADIO_TEST_TX_CARRIER:
			break;
		case RADIO_TEST_RX_CARRIER:
			break;
		case RADIO_TEST_TX_MODULATED_CARRIER:
			break;
		case RADIO_TEST_TX_CARRIER_SWEEP:
			break;
		case RADIO_TEST_TX_RANG_TEST:
			break;
		case RADIO_TEST_LED_TEST:
		    break;
		case RADIO_TEST_FACTORY_OUT_SET:
			break;
		case RADIO_TEST_SLEEP_MODE:
			break;
		case RADIO_TEST_SEND_FACTORY_CMD:
			break;
	}

}

//======= keyboard_basic_function =======
static void keyboard_basic_function(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("keyboard_basic_function\r\n");
}

//======= sensor_id =======
static void sensor_id(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("sensor_id\r\n");
}

//======= saadc_enable =======
static void saadc_enable(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("saadc_enable\r\n");
}

//======= saadc_disable =======
static void saadc_disable(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("saadc_disable\r\n");
}

//======= ber_test =======
static void ber_test(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("ber_test\r\n");
}

//======= read_rf_addr =======
static void read_rf_addr(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("read_rf_addr\r\n");
}

//======= read_flash_data =======
static void read_flash_data(uint8_t *cmd_dat ,uint8_t cmd_dat_length)
{
	uart_printf("read_flash_data\r\n");
}



static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);

	static size_t aborted_len;
	struct uart_data_t *buf;
	static uint8_t *aborted_buf;
	static bool disable_req;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("UART_TX_DONE");
		if ((evt->data.tx.len == 0) ||
		    (!evt->data.tx.buf)) {
			return;
		}

		if (aborted_buf) {
			buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
					   data[0]);
			aborted_buf = NULL;
			aborted_len = 0;
		} else {
			buf = CONTAINER_OF(evt->data.tx.buf, struct uart_data_t,
					   data[0]);
		}

		k_free(buf);

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		if (!buf) {
			return;
		}

		if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS)) {
			LOG_WRN("Failed to send data over UART");
		}

		break;

	case UART_RX_RDY:
		LOG_DBG("UART_RX_RDY");
		buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data[0]);
		buf->len += evt->data.rx.len;
		for(uint8_t i=0;i<buf->len;i++)
		{
			LOG_DBG("buf[%d] : 0x%02x", i, buf->data[i]);
		}

  		uint16_t tempSize;
  		uint16_t tempIndex;
  		if ((rx_head + buf->len) >= FACTORY_UART_RX_BUF_SIZE)
  		{
	  		tempSize = FACTORY_UART_RX_BUF_SIZE - rx_head;
	  		tempIndex = 0;
	  		memcpy(&factory_uart_rx_buf[rx_head] ,&buf->data[tempIndex] ,tempSize);
	  		tempIndex = tempSize;
	  		tempSize = buf->len - tempSize;
	  		rx_head = 0;
	  		memcpy(&factory_uart_rx_buf[rx_head] ,&buf->data[tempIndex] ,tempSize);
  		}
  		else
  		{
	  		memcpy(&factory_uart_rx_buf[rx_head] ,buf->data ,buf->len);
	  		rx_head += buf->len;
  		}

		if (disable_req) {
			return;
		}
		break;

	case UART_RX_DISABLED:
		LOG_DBG("UART_RX_DISABLED");
		disable_req = false;

		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
			k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
			return;
		}

		uart_rx_enable(uart, buf->data, sizeof(buf->data),
			       UART_WAIT_FOR_RX);

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("UART_RX_BUF_REQUEST");
		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
			uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("UART_RX_BUF_RELEASED");
		buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
				   data[0]);

		if (buf->len > 0) {
			k_fifo_put(&fifo_uart_rx_data, buf);
		} else {
			k_free(buf);
		}

		break;

	case UART_TX_ABORTED:
		LOG_DBG("UART_TX_ABORTED");
		if (!aborted_buf) {
			aborted_buf = (uint8_t *)evt->data.tx.buf;
		}

		aborted_len += evt->data.tx.len;
		buf = CONTAINER_OF((void *)aborted_buf, struct uart_data_t,
				   data);

		uart_tx(uart, &buf->data[aborted_len],
			buf->len - aborted_len, SYS_FOREVER_MS);

		break;

	default:
		break;
	}
    
}

static void uart_work_handler(struct k_work *item)
{
    struct uart_data_t *buf;
    buf = k_malloc(sizeof(*buf));
	if (buf) {
		buf->len = 0;
	} else {
		LOG_WRN("Not able to allocate UART receive buffer");
		k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
		return;
	}
}

static int uart_init(void)
{
    int err;
	int pos;
	struct uart_data_t *rx;
    LOG_ERR("uart_init");
	if (!device_is_ready(uart)) {
		return -ENODEV;
	}

	rx = k_malloc(sizeof(*rx));
	if (rx) {
		rx->len = 0;
	} else {
		return -ENOMEM;
	}

	k_work_init_delayable(&uart_work, uart_work_handler);

	err = uart_callback_set(uart, uart_cb, NULL);
	if (err) {
		k_free(rx);
		LOG_ERR("Cannot initialize UART callback");
		return err;
	}

    uart_printf("Starting UART\r\n");

	err = uart_rx_enable(uart, rx->data, sizeof(rx->data), UART_WAIT_FOR_RX);
	if (err) {
		LOG_ERR("Cannot enable uart reception (err: %d)", err);
		/* Free the rx buffer only because the tx buffer will be handled in the callback */
		k_free(rx);
	}

    return err;
}

static void uart_handler_thread_fn(void)
{
    while(true)
    {
        k_sem_take(&uart_handler_sem, K_FOREVER);
		
		power_manager_restrict((size_t)NULL, POWER_MANAGER_LEVEL_ALIVE); //keep device alive
		
		factory_rx_buf_process();
    }
}

static void init(void)
{
	factory_command_init();
    uart_init();
	thread_init();
	factory_timer_start(FACTORY_TIMER_POLLING_INTERVAL_MS);
}


static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
        LOG_ERR("app_event_handler");
        const struct module_state_event *event = cast_module_state_event(aeh);
	    if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			if(check_uart_driver_status())
			{
            	init();
			}
			module_set_state(MODULE_STATE_READY);
        }
        return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);

