/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>

#include <soc.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>

#include <hal/nrf_saadc.h>

#include <app_event_manager.h>
#include <caf/events/power_event.h>
#include "battery_event.h"
#include "battery_read_request_event.h" // wayne

#include CONFIG_DESKTOP_BATTERY_DEF_PATH

#define MODULE battery_meas
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_BATTERY_MEAS_LOG_LEVEL);

//============ ram retention =============
#if !NRF_POWER_HAS_RESETREAS
#include <hal/nrf_reset.h>
#endif

#include <zephyr/drivers/retained_mem.h>

#define awmsg 1
#ifdef awmsg
    #define awprintk(...) printk(__VA_ARGS__)
#else
    #define awprintk(...)  
#endif


#if CONFIG_RETAINED_MEM
const static struct device *retained_mem_device0 =	DEVICE_DT_GET(DT_ALIAS(retainedmemdevice0));
#define RAM_BATTERY_OFFSET    10
static uint8_t battery_buffer[10];
static uint8_t battery_lvl_sample_done_cnt; //wayne
static uint8_t RAM_BATTERY_INFO[10] = {
	0x24, 0x83, 0xa9, 0x7c, 0xdf, 0x19, 0x01, 0x00, 0x8f, 0x64, //last byte as battery level
};

void ram_battery_write(void)
{
	int32_t rc;

	rc = retained_mem_write(retained_mem_device0, RAM_BATTERY_OFFSET, RAM_BATTERY_INFO, sizeof(RAM_BATTERY_INFO));
}

void ram_battery_empty(void) //reserve, becareful incase erase other info in ram. ex. pairing flag
{
	int32_t rc;

	rc = retained_mem_clear(retained_mem_device0);
}

void ram_battery_read(void)
{
	int32_t rc;
	uint32_t reas;
	reas = nrf_reset_resetreas_get(NRF_RESET);
	if (reas & NRF_RESET_RESETREAS_SREQ_MASK) 
	{
		LOG_ERR("Reset by soft-reset, ignor power on led\n");
		memset(battery_buffer, 0, sizeof(battery_buffer));

		rc = retained_mem_read(retained_mem_device0, RAM_BATTERY_OFFSET, battery_buffer, sizeof(battery_buffer));
		
		if(memcmp(battery_buffer ,RAM_BATTERY_INFO ,(sizeof(battery_buffer)-1)) == 0)
		{
#if CONFIG_DPM_ENABLE
			extern uint8_t g_battery;
			g_battery = battery_buffer[9];
#endif
		}		
	}

}
#endif


#if (CONFIG_CY25)

#include <zephyr/drivers/adc.h>
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

 

#endif

#define ADC_NODE		DT_NODELABEL(adc)
#define ADC_RESOLUTION		12
#define ADC_OVERSAMPLING	4 /* 2^ADC_OVERSAMPLING samples are averaged */
#define ADC_MAX 		4096
#define ADC_GAIN		BATTERY_MEAS_ADC_GAIN
#define ADC_REFERENCE		ADC_REF_INTERNAL

#if (CONFIG_CY25)
#define ADC_REF_INTERNAL_MV	    900UL //nrf54l15 reference internal
#else
#define ADC_REF_INTERNAL_MV	600UL
#endif

#define ADC_ACQUISITION_TIME	ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)
#if (CONFIG_CY25KB_RECHARGEABLE || CONFIG_CY25KB_DRYCELL)

#include "charger.h" //aw
#include <stdlib.h>


static const struct device *charger_dev =
	DEVICE_DT_GET_ONE(CHARGER_COMPATIBLE);
static struct bat_charge_info_s {
	//int bat_charger_state;
    uint32_t voltage_before_charge_as_baseline;
	bool  plug_before_poweron_need_adj;
	uint32_t plug_before_poweron_need_adj_vbuf;
	uint32_t charger_plugin_tick;
	uint8_t  charger_unplug_tick;
	int  charge_table_start_index;
	uint8_t charge_adj_tick_offset;
	uint8_t real_bat_level;
	uint8_t est_bat_level;
	int8_t make_fake_level;

}bat_charge_info;
int battery_meas_bat_charger_state;
#define CHARGE_DEVIATION_NORMAL 200
#define CHARGE_DEVIATION_LOWBAT 350
#define UNCHARGE_STABLE_TIME 30   //n*BATTERY_WORK_INTERVAL
#define MAX_CHARGE_TIME      168  //n*BATTERY_WORK_INTERVAL
#define FAKE_BAT_LEVEL_GAP   1
#define FAKE_BAT_LEVEL_DROP_STEP(x)   (abs(x)>10?3:2)
static struct k_work_delayable force_battery_lvl_read;
#define POWER_ON_PLUGIN_BASEADJ_JUDGE_GAP 500
#define LOW_BAT_15S_BASE 2100
#define LOW_BAT_55S_BASE 2300
#define SET_TICK_CHECK_STEP1 3001 
#define SET_TICK_CHECK_STEP2 3300
#define SET_TICK_CHECK_STEP3 3652
static struct k_work send_charge_info;
extern uint8_t g_battery;
//aw above

#define ADC_CHANNEL_ID		7
#else
#define ADC_CHANNEL_ID		0
#endif
#define ADC_CHANNEL_INPUT	BATTERY_MEAS_ADC_INPUT
/* ADC asynchronous read is scheduled on odd works. Value read happens during
 * even works. This is done to avoid creating a thread for battery monitor.
 */
#define BATTERY_WORK_INTERVAL	(CONFIG_DESKTOP_BATTERY_MEAS_POLL_INTERVAL_MS / 2)

#if IS_ENABLED(CONFIG_DESKTOP_BATTERY_MEAS_HAS_VOLTAGE_DIVIDER)
#if (CONFIG_CY25KB_RECHARGEABLE || CONFIG_CY25KB_DRYCELL)

#define BATTERY_VOLTAGE(sample)	((float)sample * BATTERY_MEAS_VOLTAGE_GAIN	\
		* ADC_REF_INTERNAL_MV					\
		* (CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_UPPER	\
		+ CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_LOWER)	\
		/ CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_LOWER / ADC_MAX)*1.03f
#else
#define BATTERY_VOLTAGE(sample)	(sample * BATTERY_MEAS_VOLTAGE_GAIN	\
		* ADC_REF_INTERNAL_MV					\
		* (CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_UPPER	\
		+ CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_LOWER)	\
		/ CONFIG_DESKTOP_BATTERY_MEAS_VOLTAGE_DIVIDER_LOWER / ADC_MAX)
#endif
#else

#define BATTERY_VOLTAGE(sample) (sample * BATTERY_MEAS_VOLTAGE_GAIN	\
				 * ADC_REF_INTERNAL_MV / ADC_MAX)
#endif

static const struct device *const adc_dev = DEVICE_DT_GET(ADC_NODE);
static int16_t adc_buffer;
static bool adc_async_read_pending;

static struct k_work_delayable battery_lvl_read;
static struct k_poll_signal async_sig = K_POLL_SIGNAL_INITIALIZER(async_sig);
static struct k_poll_event  async_evt =
	K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
				 K_POLL_MODE_NOTIFY_ONLY,
				 &async_sig);

static const struct device *const gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static atomic_t active;
static bool sampling;

static int init_adc(void)
{
	if (!device_is_ready(adc_dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}
#if (CONFIG_CY25)
	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			awprintk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		int err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			awprintk("Could not setup channel #%d (%d)\n", i, err);
			return 0;
		}
	}
#else
	static const struct adc_channel_cfg channel_cfg = {
		.gain             = ADC_GAIN,
		.reference        = ADC_REFERENCE,
		.acquisition_time = ADC_ACQUISITION_TIME,
		.channel_id       = ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
		.input_positive   = ADC_CHANNEL_INPUT,
#endif
	};
	int err = adc_channel_setup(adc_dev, &channel_cfg);
	if (err) {
		LOG_ERR("Setting up the ADC channel failed");
		return err;
	}
#endif
#if (!CONFIG_CY25)
	/* Check if number of elements in LUT is proper */
	BUILD_ASSERT(CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL
			 - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL
			 == (ARRAY_SIZE(battery_voltage_to_soc) - 1)
			 * CONFIG_DESKTOP_VOLTAGE_TO_SOC_DELTA,
			 "Improper number of elements in lookup table");
#endif


	return 0;
}


static int battery_monitor_start(void)
{
	if (IS_ENABLED(CONFIG_DESKTOP_BATTERY_MEAS_HAS_ENABLE_PIN)) {
		int err = gpio_pin_set_raw(gpio_dev,
					   CONFIG_DESKTOP_BATTERY_MEAS_ENABLE_PIN,
					   1);
		if (err) {
			 LOG_ERR("Cannot enable battery monitor circuit");
			 return err;
		}
	}

	sampling = true;
	k_work_reschedule(&battery_lvl_read,
			      K_MSEC(BATTERY_WORK_INTERVAL));

#if CONFIG_RETAINED_MEM
	ram_battery_read();
#endif	

	return 0;
}

static void battery_monitor_stop(void)
{
	/* Cancel cannot fail if executed from another work's context. */
	(void)k_work_cancel_delayable(&battery_lvl_read);
	sampling = false;
	int err = 0;

	if (IS_ENABLED(CONFIG_DESKTOP_BATTERY_MEAS_HAS_ENABLE_PIN)) {
		err = gpio_pin_set_raw(gpio_dev,
				       CONFIG_DESKTOP_BATTERY_MEAS_ENABLE_PIN,
				       0);
		if (err) {
			LOG_ERR("Cannot disable battery monitor circuit");
			module_set_state(MODULE_STATE_ERROR);

			return;
		}
	}

	module_set_state(MODULE_STATE_STANDBY);
}

#if (CONFIG_CY25)

static bool is_bat_low = false;
static bool lvl_lp_filiter(uint8_t level)
{
	if (is_bat_low == 0 && level <= CONFIG_CY25_BAT_LOW_PERCEN) {
		is_bat_low = true;

		struct battery_state_event *event = new_battery_state_event();
		event->state = BATTERY_STATE_LOW_BAT;

		APP_EVENT_SUBMIT(event);
	}
	return true;
}
#endif

#if (CONFIG_CY25KB_RECHARGEABLE)


#if 0
#define CBT_TOTAL_IDX 139
static uint16_t charge_battery_table[CBT_TOTAL_IDX] = 
{
	0,    320,  520,  700,  820,  1000, 1140, 1200, 1600, 2060, 2200, 2400, 2460, 2500, 2540, 2580, 2620, 2660, 2680, 
	2700, 2940, 3001, 3046, 3088, 3124, 3155, 3186, 3208, 3227, 3246, 3263, 3279, 3292, 3305, 3319, 3331, 3343, 3354, 3366, 3375, 3385, 3394, 3404, 3413, 
	3423, 3431, 3439, 3447, 3455, 3463, 3470, 3477, 3485, 3492, 3498, 3505, 3513, 3519, 3525, 3531, 3537, 3543, 3549, 3555, 3561, 3566, 3571, 3577, 3582, 
	3586, 3591, 3597, 3602, 3606, 3611, 3615, 3620, 3625, 3628, 3633, 3636, 3640, 3644, 3648, 3652, 3655, 3659, 3663, 3665, 3669, 3672, 3675, 3678, 3682, 
	3686, 3688, 3691, 3694, 3696, 3698, 3701, 3703, 3705, 3708, 3710, 3712, 3714, 3716, 3718, 3720, 3722, 3724, 3726, 3728, 3730, 3731, 3733, 3734, 3735, 
	3737, 3739, 3740, 3742, 3743, 3744, 3745, 3746, 3748, 3750, 3751, 3752, 3753, 3754, 3755, 3756, 3757, 3758, 3759, 3760
};
#else
#define CBT_TOTAL_IDX 133
static uint16_t charge_battery_table[CBT_TOTAL_IDX] = 
{
	0,    520,  1260, 1500, 1520, 1860, 2400, 2460,  
	2571, 2635, 2684, 2726, 2762, 2796, 2828, 2863, 2893, 2918, 2946, 2969, 2993, 3014, 3033, 3052, 3072, 3090, 3107, 3123, 3139, 3155, 3169, 3185, 3198, 
	3213, 3226, 3238, 3251, 3262, 3274, 3286, 3299, 3310, 3320, 3331, 3343, 3352, 3363, 3372, 3382, 3391, 3400, 3410, 3418, 3427, 3436, 3444, 3452, 3460, 
	3468, 3476, 3483, 3492, 3500, 3505, 3513, 3519, 3526, 3534, 3539, 3546, 3551, 3559, 3564, 3570, 3576, 3580, 3586, 3591, 3597, 3601, 3607, 3611, 3616, 
	3620, 3625, 3629, 3633, 3637, 3641, 3645, 3649, 3652, 3656, 3660, 3663, 3666, 3669, 3673, 3675, 3678, 3682, 3685, 3687, 3690, 3692, 3695, 3698, 3700, 
	3702, 3705, 3706, 3708, 3711, 3713, 3716, 3717, 3719, 3722, 3722, 3724, 3727, 3728, 3730, 3732, 3733, 3735, 3736, 3737, 3739, 3740, 3741, 3743, 3743, 

};
#endif

static uint32_t battery_lvl_convert(void)
{
	uint32_t cv_voltage=0;
	if (bat_charge_info.charge_table_start_index < 0)
	{
		bat_charge_info.charge_table_start_index = 0;	
		for(int i=0; i<CBT_TOTAL_IDX ; i++)
		{			
			if (bat_charge_info.voltage_before_charge_as_baseline <= charge_battery_table[i])
			{
				if (i==0)
					bat_charge_info.charge_table_start_index = i;
				else
				{
					if (abs(bat_charge_info.voltage_before_charge_as_baseline - charge_battery_table[i-1]) <= abs(bat_charge_info.voltage_before_charge_as_baseline - charge_battery_table[i])) {						
						bat_charge_info.charge_table_start_index = i-1;
					} else {
						bat_charge_info.charge_table_start_index = i;
					}
				}
				break;	
			}
			 	
		}

	}

	int tick2idx = bat_charge_info.charge_table_start_index+bat_charge_info.charger_plugin_tick;
	if (tick2idx >= CBT_TOTAL_IDX)
		tick2idx = (CBT_TOTAL_IDX-1);

	cv_voltage = charge_battery_table[tick2idx];

	if(cv_voltage < SET_TICK_CHECK_STEP1)
		bat_charge_info.charge_adj_tick_offset=1;
	else if(cv_voltage < SET_TICK_CHECK_STEP2)
		bat_charge_info.charge_adj_tick_offset=2;
	else if(cv_voltage < SET_TICK_CHECK_STEP3) 
		bat_charge_info.charge_adj_tick_offset=3;
	else
		bat_charge_info.charge_adj_tick_offset=2;

	//awprintk("#aw est1: ev=%u mV\n",cv_voltage);
	return cv_voltage  ;

}

static void plug_before_poweron_check(uint32_t voltage)
{
	if (bat_charge_info.voltage_before_charge_as_baseline==0)//plug usb before poweron
	{
		bat_charge_info.plug_before_poweron_need_adj = true;
		bat_charge_info.plug_before_poweron_need_adj_vbuf = voltage;
		if(voltage<LOW_BAT_15S_BASE)//the first 15 seconds of charging
		{ 
			bat_charge_info.voltage_before_charge_as_baseline = charge_battery_table[0];
		}
		else  if(voltage<LOW_BAT_55S_BASE)//the first 86 seconds of charging
		{
			bat_charge_info.voltage_before_charge_as_baseline = charge_battery_table[2];
		}
		else
		{

			//int deviation = (voltage > 3000) ? CHARGE_DEVIATION_NORMAL : CHARGE_DEVIATION_LOWBAT;	
			int deviation = CHARGE_DEVIATION_NORMAL;			
			bat_charge_info.voltage_before_charge_as_baseline =voltage - deviation;// (( voltage - deviation) > 0) ? ( voltage - deviation) : 1;
			bat_charge_info.plug_before_poweron_need_adj = false;
		}
	}
	else if(bat_charge_info.plug_before_poweron_need_adj)
	{ 
		if((voltage - bat_charge_info.plug_before_poweron_need_adj_vbuf) > POWER_ON_PLUGIN_BASEADJ_JUDGE_GAP)
		{
			if(bat_charge_info.voltage_before_charge_as_baseline == LOW_BAT_15S_BASE)
				bat_charge_info.voltage_before_charge_as_baseline = charge_battery_table[2]; //change base line to the start from point of 20s
			else 
				bat_charge_info.voltage_before_charge_as_baseline = charge_battery_table[6]; //change base line to the start from point of 60s

			bat_charge_info.charge_table_start_index = -1;
			bat_charge_info.plug_before_poweron_need_adj = false;
			awprintk("####aw baseline adj=%d\n",bat_charge_info.voltage_before_charge_as_baseline);

		}
	}
}

static uint8_t battery_lvl_process_with_charger(uint32_t voltage) 
{

	int16_t gap =( (CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)/100);
	uint8_t level=0;
	uint8_t real_batlvl;
	 
	if (battery_meas_bat_charger_state != 1)
	{		
		 
		if (voltage > CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL) 
			real_batlvl = 100;
		else if (voltage <= CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)
			real_batlvl = 0;		
		else
			real_batlvl = (uint8_t) ((voltage - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)/gap);
		 
		
		if (bat_charge_info.charger_unplug_tick < UNCHARGE_STABLE_TIME  && bat_charge_info.charger_plugin_tick != 0)
		{
			awprintk("aw wait bat stable!\n");
			voltage = battery_lvl_convert();			
			bat_charge_info.charger_unplug_tick ++;

			
			if( bat_charge_info.charger_unplug_tick == UNCHARGE_STABLE_TIME && abs(real_batlvl - bat_charge_info.est_bat_level) > FAKE_BAT_LEVEL_GAP)							
			{
				bat_charge_info.make_fake_level = bat_charge_info.est_bat_level - real_batlvl;
				bat_charge_info.real_bat_level = real_batlvl;
				awprintk("#aw rv=%d, rl=%d, el=%d, fl=%d\n", voltage, real_batlvl, bat_charge_info.est_bat_level, bat_charge_info.make_fake_level);
			}
			else
				bat_charge_info.make_fake_level = 0;	
				
				
			if (voltage > CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL)// && bat_charge_info.make_fake_level==0)
				level = 100;
			else if (voltage <= CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)
				level = 0;
			else
				level = (uint8_t) ((voltage - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)/gap);
				
			bat_charge_info.est_bat_level = level;
			awprintk("#aw est unplug:  est ev=%d, l=%u%%\n",voltage, level);


		}
		else
		{
			
			bat_charge_info.charge_table_start_index = -1;
			bat_charge_info.charger_plugin_tick = 0;
			bat_charge_info.voltage_before_charge_as_baseline = voltage;

			if(bat_charge_info.make_fake_level!=0)
			{
				uint8_t highlvl,lowlvl;
				
				awprintk("need fake lvl!\n");
				
				
				if(bat_charge_info.make_fake_level>0)// est > real
				{					
					level = bat_charge_info.est_bat_level - ((bat_charge_info.real_bat_level-real_batlvl)*FAKE_BAT_LEVEL_DROP_STEP(bat_charge_info.make_fake_level));;
					level = level>=0?level:0;
					awprintk("#aw fk e>r: el=%d, lrl=%d, rl=%d,  fkl=%d cvl=%d \n",bat_charge_info.est_bat_level,bat_charge_info.real_bat_level, real_batlvl,bat_charge_info.make_fake_level,level);
					lowlvl = real_batlvl;
					highlvl = level;
					
				}
				else
				{
					
					level = bat_charge_info.est_bat_level - ((bat_charge_info.real_bat_level-real_batlvl)/FAKE_BAT_LEVEL_DROP_STEP(bat_charge_info.make_fake_level));
				    level = level>=0?level:0;
					awprintk("#aw fk r>e: el=%d, lrl=%d, rl=%d,  fkl=%d cvl=%d \n",bat_charge_info.est_bat_level,bat_charge_info.real_bat_level, real_batlvl,bat_charge_info.make_fake_level,level);
					lowlvl = level ;
					highlvl = real_batlvl;
				}

				if(highlvl <= lowlvl)
				{
					level = real_batlvl;
					bat_charge_info.make_fake_level=0;
					bat_charge_info.real_bat_level = real_batlvl;
				}
			
					
							

			}
			else
			{
				level = real_batlvl;
			}


	
		}


	}
	else //charger plug-in
	{
		bat_charge_info.charger_unplug_tick = 0;
		bat_charge_info.make_fake_level = 0;

		plug_before_poweron_check(voltage);
		
		
		
			
		
		if (bat_charge_info.charger_plugin_tick >= MAX_CHARGE_TIME)//over charge
		{
			//level = (uint8_t) ((voltage - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)/gap);
			voltage = CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL;
		}
		else
		{

			voltage = battery_lvl_convert();	
			bat_charge_info.charger_plugin_tick += bat_charge_info.charge_adj_tick_offset;		

		}

		if (voltage >= CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL) 
		{
			level = 100;
		}
		else if (voltage <= CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL) 
		{
			level = 0;
		}
		else
		{
			level = (uint8_t) ((voltage - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL)/gap);
		}
		bat_charge_info.est_bat_level = level;
		awprintk("#aw est plug-in: stv=%u mV, est ev=%d, l=%u%% ,adj=%d\n",bat_charge_info.voltage_before_charge_as_baseline,voltage, level ,bat_charge_info.charge_adj_tick_offset);


		
		
	}

	return level;

}

#endif

static void battery_lvl_process(void)
{
#if (CONFIG_CY25KB_RECHARGEABLE || CONFIG_CY25KB_DRYCELL)
	//uint32_t voltage = BATTERY_VOLTAGE(adc_buffer)+ 0.5f;
	uint32_t voltage = BATTERY_VOLTAGE(adc_buffer); 
#else
	uint32_t voltage = BATTERY_VOLTAGE(adc_buffer);
#endif

	uint8_t level;

#if (CONFIG_CY25KB_RECHARGEABLE )
	level =	battery_lvl_process_with_charger(voltage);
#else

	if (voltage > CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL) {
		level = 100;
	} else if (voltage <= CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL) {
		level = 0;
		LOG_WRN("Low battery");
	} else {
#if (CONFIG_CY25)
		level = 0;

		for(uint8_t i=0; i<101; i++)
		{
			if(voltage <= battery_table[i])
			{
				level = i;
				break;
			}
		}

#else
		//awprintk("b_low=%d, b_high=%d\n",CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL,CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL);
	
		/* Using lookup table to convert voltage[mV] to SoC[%] */
		size_t lut_id = (voltage - CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL
				 + (CONFIG_DESKTOP_VOLTAGE_TO_SOC_DELTA >> 1))
				 / CONFIG_DESKTOP_VOLTAGE_TO_SOC_DELTA;
		level = battery_voltage_to_soc[lut_id];
#endif
	}
#endif
	//awprintk("adc buffer= %d\n",adc_buffer);
	//awprintk("b_low=%d, b_high=%d\n",CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL,CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL);
	if (lvl_lp_filiter(level)) {
	struct battery_level_event *event = new_battery_level_event();
	event->level = level;
  #if (CONFIG_CY25KB_RECHARGEABLE )	
	event->is_charging =(battery_meas_bat_charger_state==1)?true:false;
  #endif	
#if CONFIG_RETAINED_MEM
	RAM_BATTERY_INFO[9] = level;
    ram_battery_write();
#endif
	APP_EVENT_SUBMIT(event);
	}
	LOG_INF("Battery level: %u%% (%u mV)\n", level, voltage);
}

static int battery_adc_read(void)
{
	int err;
	static const struct adc_sequence sequence = {
		.options	= NULL,
		.channels	= BIT(ADC_CHANNEL_ID),
		.buffer		= &adc_buffer,
		.buffer_size	= sizeof(adc_buffer),
		.resolution	= ADC_RESOLUTION,
		.oversampling	= ADC_OVERSAMPLING,
		.calibrate	= false,
	};
	static const struct adc_sequence sequence_calibrate = {
		.options	= NULL,
		.channels	= BIT(ADC_CHANNEL_ID),
		.buffer		= &adc_buffer,
		.buffer_size	= sizeof(adc_buffer),
		.resolution	= ADC_RESOLUTION,
		.oversampling	= ADC_OVERSAMPLING,
		.calibrate	= true,
	};

	static bool calibrated;

	if (likely(calibrated)) {
		err = adc_read_async(adc_dev, &sequence, &async_sig);
	} else {
		err = adc_read_async(adc_dev, &sequence_calibrate,
					 &async_sig);
		calibrated = true;
	}

	return err;
}

static void battery_lvl_read_fn(struct k_work *work)
{
	int err;
#if (CONFIG_CY25KB_RECHARGEABLE)
	if (k_work_delayable_is_pending(&force_battery_lvl_read)) {
		(void)k_work_cancel_delayable(&force_battery_lvl_read);
	}
#endif
	if (!adc_async_read_pending) {
		err = battery_adc_read();

		if (err) {
			LOG_WRN("Battery level async read failed");
		} else {
			adc_async_read_pending = true;
		}
	} else {
		err = k_poll(&async_evt, 1, K_NO_WAIT);
		if (err) {
			LOG_WRN("Battery level poll failed");
		} else {
			adc_async_read_pending = false;
			battery_lvl_process();					
		}
	}

	if (atomic_get(&active) || adc_async_read_pending) {
        //Wayne 
    	if( battery_lvl_sample_done_cnt < 1 ){        
		   battery_lvl_sample_done_cnt ++; 
		   k_work_reschedule(&battery_lvl_read, K_MSEC(10));	
        }
        else {  
           k_work_reschedule(&battery_lvl_read,
 	       K_MSEC(BATTERY_WORK_INTERVAL));
        }
	    //Wayne	
   //		k_work_reschedule(&battery_lvl_read,
   //				      K_MSEC(BATTERY_WORK_INTERVAL));
	} else {
		battery_monitor_stop();
	}
}
#if (CONFIG_CY25KB_RECHARGEABLE)

static void battery_lvl_read_force(struct k_work *work)
{
	
	int err = battery_adc_read();
	if (err) {
		LOG_WRN("Battery level force read failed");
		k_work_reschedule(&force_battery_lvl_read, K_MSEC(5)); 
	}
	else{

		int retry=2;
		int i=0;
		for( i=0 ; i<retry; i++)
		{
			if(adc_buffer>0)
			{
				
				uint32_t voltage = BATTERY_VOLTAGE(adc_buffer);
				g_battery =  battery_lvl_process_with_charger(voltage);
				plug_before_poweron_check(voltage);
				awprintk("#aw battery_lvl_read_force get %dmv, lv=%d\n",voltage,g_battery);
				break;
			}
		}
		if(i==retry) 
		{
			k_work_reschedule(&force_battery_lvl_read, K_MSEC(1)); 
		}
	 
	}

}


void direct_send_charge_handler(struct k_work *work)
{
	struct battery_state_event *event = new_battery_state_event();
	if(battery_meas_bat_charger_state)
		event->state = BATTERY_STATE_CHARGING;
	else
		event->state = BATTERY_STATE_IDLE;

	APP_EVENT_SUBMIT(event);
}

static uint32_t battery_is_in_charger(const struct device *dev, int int_input_state )
{
	
	battery_meas_bat_charger_state = int_input_state;
	k_work_submit(&send_charge_info);

	return 0;
	
}
#endif
static int init_fn(void)
{
	int err = 0;
	 

	if (IS_ENABLED(CONFIG_DESKTOP_BATTERY_MEAS_HAS_ENABLE_PIN)) {
		if (!device_is_ready(gpio_dev)) {
			LOG_ERR("GPIO device not ready");
			err = -ENODEV;
			goto error;
		}

		/* Enable battery monitoring */
		err = gpio_pin_configure(gpio_dev,
					 CONFIG_DESKTOP_BATTERY_MEAS_ENABLE_PIN,
					 GPIO_OUTPUT);
	}

	if (!err) {
		err = init_adc();
	}

	if (err) {
		goto error;
	}

	k_work_init_delayable(&battery_lvl_read, battery_lvl_read_fn);
	k_work_init_delayable(&force_battery_lvl_read, battery_lvl_read_force);

#if (CONFIG_CY25KB_RECHARGEABLE)
	union charger_propval val ;
	bat_charge_info.charger_plugin_tick = 0;
	bat_charge_info.charger_unplug_tick = 0;
	bat_charge_info.voltage_before_charge_as_baseline = 0; 
	bat_charge_info.plug_before_poweron_need_adj_vbuf =0;
	bat_charge_info.plug_before_poweron_need_adj = false;
	bat_charge_info.charge_table_start_index = -1;
	bat_charge_info.charge_adj_tick_offset = 1;
	bat_charge_info.real_bat_level=0;
	bat_charge_info.est_bat_level=0;
	bat_charge_info.make_fake_level=0;
	
	//k_work_reschedule(&battery_lvl_read,K_MSEC(0));
	k_work_init(&send_charge_info, direct_send_charge_handler);
	val.system_voltage_notification = (uint32_t)((uint32_t*)(battery_is_in_charger));
	err = charger_set_prop(charger_dev, CHARGER_PROP_SYSTEM_VOLTAGE_NOTIFICATION_UV, &val);
	charger_get_prop(charger_dev, CHARGER_PROP_STATUS, &val);
	if (val.status == CHARGER_STATUS_CHARGING)
	{
		battery_meas_bat_charger_state = 1;	
	}
	else
	{
		battery_meas_bat_charger_state = 0;
	}
	k_work_reschedule(&force_battery_lvl_read, K_MSEC(400)); 
#endif

	

	err = battery_monitor_start();
error:
	return err;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			int err = init_fn();
			
			if (err) {
				module_set_state(MODULE_STATE_ERROR);
			} else {
				module_set_state(MODULE_STATE_READY);
				atomic_set(&active, true);
			}

			return false;
		}

		return false;
	}

	if (is_wake_up_event(aeh)) {
		if (!atomic_get(&active)) {
			atomic_set(&active, true);

			int err = battery_monitor_start();

			if (!err) {
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}

		return false;
	}

	if (is_power_down_event(aeh)) {
		if (atomic_get(&active)) {
			atomic_set(&active, false);

			if (adc_async_read_pending) {
				__ASSERT_NO_MSG(sampling);
				/* Poll ADC and postpone shutdown */
				k_work_reschedule(&battery_lvl_read,
						      K_MSEC(0));
			} else {
				/* No ADC sample left to read, go to standby */
				battery_monitor_stop();
			}
		}

		return sampling;
	}

	if (is_battery_read_request_event(aeh)) {
		if (atomic_get(&active)) {
		   k_work_reschedule(&battery_lvl_read, K_MSEC(10));
            battery_lvl_sample_done_cnt = 0;
          return false;
		}
    }

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event);
APP_EVENT_SUBSCRIBE(MODULE, wake_up_event);
APP_EVENT_SUBSCRIBE(MODULE, battery_read_request_event); //Wayne