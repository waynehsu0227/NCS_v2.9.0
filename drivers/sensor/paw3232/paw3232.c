/*
 * Copyright (c) 2019-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #define DT_DRV_COMPAT pixart_paw3232

 #include <zephyr/kernel.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/spi.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/sys/byteorder.h>
 #include <sensor/paw3232.h>
 
 #include <zephyr/pm/device.h>
 #include <zephyr/pm/device_runtime.h>
 
 #include <zephyr/logging/log.h>
 LOG_MODULE_REGISTER(paw3232, CONFIG_PAW3232_LOG_LEVEL);
 
 /* Timings defined by spec */
 #define T_NCS_SCLK	1			/* 120 ns */
 #define T_SRX		(20 - T_NCS_SCLK)	/* 20 us */
 #define T_SCLK_NCS_WR	(35 - T_NCS_SCLK)	/* 35 us */
 #define T_SWX		(180 - T_SCLK_NCS_WR)	/* 180 us */
 #define T_SRAD		10			/* 160 us */
 
 
 /* Sensor registers */
 #define PAW3232_REG_PRODUCT_ID		0x00
 #define PAW3232_REG_REVISION_ID		0x01
 #define PAW3232_REG_MOTION		0x02
 #define PAW3232_REG_DELTA_X_LOW		0x03
 #define PAW3232_REG_DELTA_Y_LOW		0x04
 #define PAW3232_REG_DELTA_X_HIGH		0x11
 #define PAW3232_REG_DELTA_Y_HIGH		0x12
 #define PAW3232_REG_OPERATION_MODE	0x05
 #define PAW3232_REG_CONFIGURATION	0x06
 #define PAW3232_REG_WRITE_PROTECT	0x09
 #define PAW3232_REG_MOUSE_OPTION	0x20
 #define PAW3232_REG_SLEEP1		0x0A
 #define PAW3232_REG_SLEEP2		0x0B
 #define PAW3232_REG_SLEEP3		0x07
 #define PAW3232_REG_CPI_XL		0x0D
 #define PAW3232_REG_CPI_YL		0x0E
 #define PAW3232_REG_CPI_XH		0x1D
 #define PAW3232_REG_CPI_YH		0x1E
 
 /* CPI */
 #define PAW3232_CPI_STEP_X		30u  
 #define PAW3232_CPI_STEP_Y		30u  
 #define PAW3232_CPI_MIN			100  
 #define PAW3232_CPI_MAX			6000  
 
 /* Sleep */
 #define PAW3232_SLP_ENH_POS		4u
 #define PAW3232_SLP2_ENH_POS		3u
 #define PAW3232_SLP3_ENH_POS		5u
 
 #define PAW3232_ETM_POS			0u
 #define PAW3232_ETM_SIZE		4u
 #define PAW3232_FREQ_POS		(PAW3232_ETM_POS + PAW3232_ETM_SIZE)
 #define PAW3232_FREQ_SIZE		4u
 
 #define PAW3232_ETM_MIN			0u
 #define PAW3232_ETM_MAX			BIT_MASK(PAW3232_ETM_SIZE)
 #define PAW3232_ETM_MASK		(PAW3232_ETM_MAX << PAW3232_ETM_POS)
 
 #define PAW3232_FREQ_MIN		0u
 #define PAW3232_FREQ_MAX		BIT_MASK(PAW3232_FREQ_SIZE)
 #define PAW3232_FREQ_MASK		(PAW3232_FREQ_MAX << PAW3232_FREQ_POS)
 
 /* Motion status bits */
 #define PAW3232_MOTION_STATUS_MOTION	BIT(7)
 #define PAW3232_MOTION_STATUS_DYOVF	BIT(4)
 #define PAW3232_MOTION_STATUS_DXOVF	BIT(3)
 
 /* Configuration bits */
 #define PAW3232_CONFIG_CHIP_RESET	BIT(7)
 
 /* Mouse option bits */
 #define PAW3232_MOUSE_OPT_XY_SWAP	BIT(2)
 #define PAW3232_MOUSE_OPT_Y_INV		BIT(0)
 #define PAW3232_MOUSE_OPT_X_INV		BIT(1)
 
 /* Convert deltas to x and y */
 #define PAW3232_DELTA_X(x_high, x_low)	((x_high << 8) | (x_low))
 #define PAW3232_DELTA_Y(y_high, y_low)	((y_high << 8) | (y_low))
 
 /* Sensor identification values */
 #define PAW3232_PRODUCT_ID		0x33
 
 /* Write protect magic */
 #define PAW3232_WPMAGIC			0x5A
 
 #define SPI_WRITE_BIT			BIT(7)
 
 /* Helper macros used to convert sensor values. */
 #define PAW3232_SVALUE_TO_CPI(svalue) ((uint32_t)(svalue).val1)
 #define PAW3232_SVALUE_TO_TIME(svalue) ((uint32_t)(svalue).val1)
 #define PAW3232_SVALUE_TO_BOOL(svalue) ((svalue).val1 != 0)
 
 enum async_init_step {
	 ASYNC_INIT_STEP_POWER_UP,
	 ASYNC_INIT_STEP_VERIFY_ID,
	 ASYNC_INIT_STEP_LOW_VOLTAGE,
	 ASYNC_INIT_STEP_CONFIGURE,
 
	 ASYNC_INIT_STEP_COUNT
 };
 
 struct paw3232_data {
	 const struct device          *dev;
	 struct gpio_callback         irq_gpio_cb;
	 struct k_spinlock            lock;
	 int16_t                      x;
	 int16_t                      y;
	 sensor_trigger_handler_t     data_ready_handler;
	 struct k_work                trigger_handler_work;
	 struct k_work_delayable      init_work;
	 enum async_init_step         async_init_step;
	 int                          err;
	 bool                         ready;
 };
 
 struct paw3232_config {
	 struct gpio_dt_spec irq_gpio;
	 struct spi_dt_spec bus;
	 struct gpio_dt_spec cs_gpio;
 };
 
 static const int32_t async_init_delay[ASYNC_INIT_STEP_COUNT] = {
	 [ASYNC_INIT_STEP_POWER_UP]  = 1,
	 [ASYNC_INIT_STEP_VERIFY_ID] = 0,
	 [ASYNC_INIT_STEP_LOW_VOLTAGE] = 0,
	 [ASYNC_INIT_STEP_CONFIGURE] = 0,
 };
 
 
 static int paw3232_async_init_power_up(const struct device *dev);
 static int paw3232_async_init_verify_id(const struct device *dev);
 static int paw3232_async_init_low_voltage(const struct device *dev);
 static int paw3232_async_init_configure(const struct device *dev);
 
 
 static int (* const async_init_fn[ASYNC_INIT_STEP_COUNT])(const struct device *dev) = {
	 [ASYNC_INIT_STEP_POWER_UP]  = paw3232_async_init_power_up,
	 [ASYNC_INIT_STEP_VERIFY_ID] = paw3232_async_init_verify_id,
	 [ASYNC_INIT_STEP_LOW_VOLTAGE] = paw3232_async_init_low_voltage,
	 [ASYNC_INIT_STEP_CONFIGURE] = paw3232_async_init_configure,
 };
 
 
 static int spi_cs_ctrl(const struct device *dev, bool enable)
 {
	 const struct paw3232_config *config = dev->config;
	 int err;
 
	 if (!enable) {
		 k_busy_wait(T_NCS_SCLK);
	 }
 
	 //const struct gpio_dt_spec *spec = &config->cs_gpio;
 
	 //LOG_ERR("rex cs pin: %u,enable = %u", spec->pin, enable);
 
	 err = gpio_pin_set_dt(&config->cs_gpio, (int)enable);
 
	 if (err) {
		 LOG_ERR("SPI CS ctrl failed");
	 }
 
	 if (enable) {
		 //k_busy_wait(T_NCS_SCLK);
	 }
 
	 return err;
 }
 #define GPIO_TEST false
 #if GPIO_TEST
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/devicetree.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/gpio.h>
 static const struct device * const gpio_devs[] = {
	 DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0)),
	 DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1)),
	 DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio2)),
 };
 #endif
 
 
 static int reg_read(const struct device *dev, uint8_t reg, uint8_t *buf)
 {
	 int err;
	 const struct paw3232_config *config = dev->config;
	 const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi00));
	 __ASSERT_NO_MSG((reg & SPI_WRITE_BIT) == 0);
 
 #if GPIO_TEST
	 NRF_P2->OUT = 0x440;
 #else
	 err = spi_cs_ctrl(dev, true);
	 if (err) {
		 return err;
	 }
 #endif
	 /* Write register address. */
	 const struct spi_buf tx_buf = {
		 .buf = &reg,
		 .len = 1
	 };
	 const struct spi_buf_set tx = {
		 .buffers = &tx_buf,
		 .count = 1
	 };
 
	 err = spi_write_dt(&config->bus, &tx);
	 if (err) {
		 LOG_ERR("Reg read failed on SPI write");
		 return err;
	 }
 
	 //k_busy_wait(T_SRAD);
 
	 /* Read register value. */
	 struct spi_buf rx_buf = {
		 .buf = buf,
		 .len = 1,
	 };
	 const struct spi_buf_set rx = {
		 .buffers = &rx_buf,
		 .count = 1,
	 };
 
	 err = spi_read_dt(&config->bus, &rx);
	 if (err) {
		 LOG_ERR("Reg read failed on SPI read");
		 return err;
	 }
 
 #if GPIO_TEST
	 NRF_P2->OUT = 0x60;
 #else
	 err = spi_cs_ctrl(dev, false);
	 if (err) {
		 return err;
	 }
 #endif
 #if GPIO_TEST
	 NRF_P2->PIN_CNF[2] = 2;
	 NRF_P2->PIN_CNF[4] = 2;
 #endif
 
	 //k_busy_wait(T_SRX);
 
	 return 0;
 }
 
 static int reg_write(const struct device *dev, uint8_t reg, uint8_t val)
 {
	 int err;
	 const struct paw3232_config *config = dev->config;
 
	 __ASSERT_NO_MSG((reg & SPI_WRITE_BIT) == 0);
 
	 err = spi_cs_ctrl(dev, true);
	 if (err) {
		 return err;
	 }
 
	 uint8_t buf[] = {
		 SPI_WRITE_BIT | reg,
		 val
	 };
	 const struct spi_buf tx_buf = {
		 .buf = buf,
		 .len = ARRAY_SIZE(buf)
	 };
	 const struct spi_buf_set tx = {
		 .buffers = &tx_buf,
		 .count = 1
	 };
 
	 err = spi_write_dt(&config->bus, &tx);
	 if (err) {
		 LOG_ERR("Reg write failed on SPI write");
		 return err;
	 }
 
	 k_busy_wait(T_SCLK_NCS_WR);
 
	 err = spi_cs_ctrl(dev, false);
	 if (err) {
		 return err;
	 }
 
	 k_busy_wait(T_SWX);
 
	 return 0;
 }
 
 static int update_cpi(const struct device *dev, uint32_t cpi)
 {
	 int err;

	 if ((cpi > PAW3232_CPI_MAX) || (cpi < PAW3232_CPI_MIN)) {
		 LOG_ERR("CPI %" PRIu32 " out of range", cpi);
		 return -EINVAL;
	 }
 
	 uint16_t CPI_X = (uint16_t)(cpi / 0.622);
	 uint16_t CPI_Y = (uint16_t)(cpi / 0.613);
 

	 uint8_t CPI_XH = (CPI_X >> 8) & 0x3F;  
	 uint8_t CPI_XL = CPI_X & 0xFF;         
	 uint8_t CPI_YH = (CPI_Y >> 8) & 0x3F;  
	 uint8_t CPI_YL = CPI_Y & 0xFF;         
 
 
	 LOG_ERR("Set CPI: %u (requested: %u, reg:0x%" PRIx8 ")",
		    cpi, cpi, CPI_X);
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, PAW3232_WPMAGIC);
	 if (err) {
		 LOG_ERR("Cannot disable write protect");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_CPI_XL, CPI_XL);
	 if (err) {
		 LOG_ERR("Failed to change xl CPI");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_CPI_YL, CPI_YL);
	 if (err) {
		 LOG_ERR("Failed to change yl CPI");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_CPI_XH, CPI_XH);
	 if (err) {
		 LOG_ERR("Failed to change xh CPI");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_CPI_YH, CPI_YH);
	 if (err) {
		 LOG_ERR("Failed to change yh CPI");
		 return err;
	 }

	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 if (err) {
		 LOG_ERR("Cannot enable write protect");
	 }

	 return err;
 }
 
 static int update_sleep_timeout(const struct device *dev, uint8_t reg_addr,
				 uint32_t timeout_ms)
 {
	 uint32_t timeout_step_ms;
 
	 switch (reg_addr) {
	 case PAW3232_REG_SLEEP1:
		 timeout_step_ms = 32;
		 break;
 
	 case PAW3232_REG_SLEEP2:
	 case PAW3232_REG_SLEEP3:
		 timeout_step_ms = 8000;
		 break;
 
	 default:
		 LOG_ERR("Not supported");
		 return -ENOTSUP;
	 }
 
	 uint32_t etm = timeout_ms / timeout_step_ms - 1;
 
	 if ((etm < PAW3232_ETM_MIN) || (etm > PAW3232_ETM_MAX)) {
		 LOG_WRN("Sleep timeout %" PRIu32 " out of range", timeout_ms);
		 return -EINVAL;
	 }
 
	 LOG_DBG("Set sleep 0x%02x timeout: %u (requested: %u, reg:0x%" PRIx8 ")",
		 reg_addr,
		 (etm + 1) * timeout_step_ms,
		 timeout_ms,
		 etm);
 
	 int err;
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, PAW3232_WPMAGIC);
	 if (err) {
		 LOG_ERR("Cannot disable write protect");
		 return err;
	 }
 
	 uint8_t regval;
 
	 err = reg_read(dev, reg_addr, &regval);
	 if (err) {
		 LOG_ERR("Failed to read sleep register");
		 return err;
	 }
 
	 __ASSERT_NO_MSG((etm & PAW3232_ETM_MASK) == etm);
 
	 regval &= ~PAW3232_ETM_MASK;
	 regval |= (etm << PAW3232_ETM_POS);
 
	 err = reg_write(dev, reg_addr, regval);
	 if (err) {
		 LOG_ERR("Failed to change sleep time");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 if (err) {
		 LOG_ERR("Cannot enable write protect");
	 }
 
	 return err;
 }
 
 static int update_sample_time(const struct device *dev, uint8_t reg_addr,
				   uint32_t sample_time_ms)
 {
	 uint32_t sample_time_step;
	 uint32_t sample_time_min;
	 uint32_t sample_time_max;
 
	 switch (reg_addr) {
	 case PAW3232_REG_SLEEP1:
		 sample_time_step = 4;
		 sample_time_min = 4;
		 sample_time_max = 64;
		 break;
 
	 case PAW3232_REG_SLEEP2:
	 case PAW3232_REG_SLEEP3:
		 sample_time_step = 64;
		 sample_time_min = 64;
		 sample_time_max = 1024;
		 break;
 
	 default:
		 LOG_ERR("Not supported");
		 return -ENOTSUP;
	 }
 
	 if ((sample_time_ms > sample_time_max) ||
		 (sample_time_ms < sample_time_min))	{
		 LOG_WRN("Sample time %" PRIu32 " out of range", sample_time_ms);
		 return -EINVAL;
	 }
 
	 uint8_t reg_freq = (sample_time_ms - sample_time_min) / sample_time_step;
 
	 LOG_DBG("Set sleep 0x%02x sample time: %u (requested: %u, reg:0x%" PRIx8 ")",
		 reg_addr,
		 (reg_freq * sample_time_step) + sample_time_min,
		 sample_time_ms,
		 reg_freq);
 
	 __ASSERT_NO_MSG(reg_freq <= PAW3232_FREQ_MAX);
 
	 int err;
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, PAW3232_WPMAGIC);
	 if (err) {
		 LOG_ERR("Cannot disable write protect");
		 return err;
	 }
 
	 uint8_t regval;
 
	 err = reg_read(dev, reg_addr, &regval);
	 if (err) {
		 LOG_ERR("Failed to read sleep register");
		 return err;
	 }
 
	 regval &= ~PAW3232_FREQ_MASK;
	 regval |= (reg_freq << PAW3232_FREQ_POS);
 
	 err = reg_write(dev, reg_addr, regval);
	 if (err) {
		 LOG_ERR("Failed to change sample time");
		 return err;
	 }
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 if (err) {
		 LOG_ERR("Cannot enable write protect");
	 }
 
	 return err;
 }
 
 static int toggle_sleep_modes(const struct device *dev, uint8_t reg_addr1,
				   uint8_t reg_addr2, bool enable)
 {
	 int err = reg_write(dev, PAW3232_REG_WRITE_PROTECT,
				 PAW3232_WPMAGIC);
	 if (err) {
		 LOG_ERR("Cannot disable write protect");
		 return err;
	 }
 #if 0
	 uint8_t regval;
 
	 LOG_DBG("%sable sleep", (enable) ? ("En") : ("Dis"));
 
	 /* Sleep 1 and Sleep 2 */
	 err = reg_read(dev, reg_addr1, &regval);
	 if (err) {
		 LOG_ERR("Failed to read operation mode register");
		 return err;
	 }
 
	 uint8_t sleep_enable_mask = BIT(PAW3232_SLP_ENH_POS) |
				  BIT(PAW3232_SLP2_ENH_POS);
 
	 if (enable) {
		 regval |= sleep_enable_mask;
	 } else {
		 regval &= ~sleep_enable_mask;
	 }
 
	 err = reg_write(dev, reg_addr1, regval);
	 if (err) {
		 LOG_ERR("Failed to %sable sleep", (enable) ? ("en") : ("dis"));
		 return err;
	 }
 
	 /* Sleep 3 */
	 err = reg_read(dev, reg_addr2, &regval);
	 if (err) {
		 LOG_ERR("Failed to read configuration register");
		 return err;
	 }
 
	 sleep_enable_mask = BIT(PAW3232_SLP3_ENH_POS);
 
	 if (enable) {
		 regval |= sleep_enable_mask;
	 } else {
		 regval &= ~sleep_enable_mask;
	 }
 
	 err = reg_write(dev, reg_addr2, regval);
	 if (err) {
		 LOG_ERR("Failed to %sable sleep", (enable) ? ("en") : ("dis"));
		 return err;
	 }
 #endif
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 if (err) {
		 LOG_ERR("Cannot enable write protect");
	 }
 
	 return err;
 }
 
 static void irq_handler(const struct device *gpiob, struct gpio_callback *cb,
			 uint32_t pins)
 {
	 int err;
	 struct paw3232_data *data = CONTAINER_OF(cb, struct paw3232_data,
						  irq_gpio_cb);
	 const struct device *dev = data->dev;
	 const struct paw3232_config *config = dev->config;
 
	 err = gpio_pin_interrupt_configure_dt(&config->irq_gpio,
						   GPIO_INT_DISABLE);
	 if (unlikely(err)) {
		 LOG_ERR("Cannot disable IRQ");
		 k_panic();
	 }
 
	 k_work_submit(&data->trigger_handler_work);
 }
 
 static void trigger_handler(struct k_work *work)
 {
	 sensor_trigger_handler_t handler;
	 int err = 0;
	 struct paw3232_data *data = CONTAINER_OF(work, struct paw3232_data,
							  trigger_handler_work);
	 const struct device *dev = data->dev;
	 const struct paw3232_config *config = dev->config;
 
	 k_spinlock_key_t key = k_spin_lock(&data->lock);
 
	 handler = data->data_ready_handler;
	 k_spin_unlock(&data->lock, key);
 
	 if (!handler) {
		 return;
	 }
 
	 struct sensor_trigger trig = {
		 .type = SENSOR_TRIG_DATA_READY,
		 .chan = SENSOR_CHAN_ALL,
	 };
 
	 handler(dev, &trig);
 
	 key = k_spin_lock(&data->lock);
	 if (data->data_ready_handler) {
		 err = gpio_pin_interrupt_configure_dt(&config->irq_gpio,
							   GPIO_INT_LEVEL_ACTIVE);
	 }
	 k_spin_unlock(&data->lock, key);
 
	 if (unlikely(err)) {
		 LOG_ERR("Cannot re-enable IRQ");
		 k_panic();
	 }
 }
 
 static int paw3232_async_init_power_up(const struct device *dev)
 {
	 return reg_write(dev, PAW3232_REG_CONFIGURATION,
			  PAW3232_CONFIG_CHIP_RESET);
 }
 
 static int paw3232_async_init_verify_id(const struct device *dev)
 {
	 int err;
 
	 uint8_t product_id;
	 err = reg_read(dev, PAW3232_REG_PRODUCT_ID, &product_id);
	 if (err) {
		 LOG_ERR("Cannot obtain product ID");
		 return err;
	 }
 
	 LOG_DBG("Product ID: 0x%" PRIx8, product_id);
	 if (product_id != PAW3232_PRODUCT_ID) {
		 LOG_ERR("Invalid product ID (0x%" PRIx8")!", product_id);
		 return -EIO;
	 }
 
	 return err;
 }
 
 static int paw3232_async_init_low_voltage(const struct device *dev)
 {
	 int err;
 
	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, PAW3232_WPMAGIC);
	 if (!err) {
		err = reg_write(dev, 0x7F, 0x00);
		err = reg_write(dev, 0x09, 0x5A);
		err = reg_write(dev, 0x20, 0x03);
		err = reg_write(dev, 0x1D, 0x06);
		err = reg_write(dev, 0x0C, 0x48);
		err = reg_write(dev, 0x1E, 0x06);
		err = reg_write(dev, 0x0D, 0x5F);
		err = reg_write(dev, 0x4E, 0x00);
		err = reg_write(dev, 0x7F, 0x02);
		err = reg_write(dev, 0x4F, 0x21);
		err = reg_write(dev, 0x7F, 0x03);
		err = reg_write(dev, 0x2B, 0x18);
		err = reg_write(dev, 0x2C, 0x5A);
		err = reg_write(dev, 0x31, 0x24);
		err = reg_write(dev, 0x12, 0x44);
		err = reg_write(dev, 0x1A, 0x30);
		err = reg_write(dev, 0x1B, 0x10);
		err = reg_write(dev, 0x25, 0x38);
		err = reg_write(dev, 0x27, 0x3C);
		err = reg_write(dev, 0x34, 0x0C);
		err = reg_write(dev, 0x35, 0x10);
		err = reg_write(dev, 0x37, 0x1C);
		err = reg_write(dev, 0x38, 0x28);
		err = reg_write(dev, 0x40, 0x61);
		err = reg_write(dev, 0x41, 0x76);
		err = reg_write(dev, 0x56, 0x5D);
		err = reg_write(dev, 0x63, 0x05);
		err = reg_write(dev, 0x2E, 0x46);
		err = reg_write(dev, 0x58, 0x30);
		err = reg_write(dev, 0x7F, 0x00);
		err = reg_write(dev, 0x4E, 0x01);
		err = reg_write(dev, 0x05, 0x31);		
	 }
	 if (!err) {
		 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 }
 
	 return err;
 }
 
 
 static int paw3232_async_init_configure(const struct device *dev)
 {
	 int err;

	 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, PAW3232_WPMAGIC);
	 if (!err) {
		 uint8_t mouse_option = 0;
 
		 if (IS_ENABLED(CONFIG_PAW3232_ORIENTATION_90)) {
			 mouse_option |= PAW3232_MOUSE_OPT_XY_SWAP |
					 PAW3232_MOUSE_OPT_Y_INV;
		 } else if (IS_ENABLED(CONFIG_PAW3232_ORIENTATION_180)) {
			 mouse_option |= PAW3232_MOUSE_OPT_Y_INV |
					 PAW3232_MOUSE_OPT_X_INV;
		 } else if (IS_ENABLED(CONFIG_PAW3232_ORIENTATION_270)) {
			 mouse_option |= PAW3232_MOUSE_OPT_XY_SWAP |
					 PAW3232_MOUSE_OPT_X_INV;
		 }
 
		 err = reg_write(dev, PAW3232_REG_MOUSE_OPTION,
				 mouse_option);
	 }
	 if (!err) {
		 err = reg_write(dev, PAW3232_REG_WRITE_PROTECT, 0);
	 }

	 return err;
 }
 
 static void paw3232_async_init(struct k_work *work)
 {
	 struct paw3232_data *data = CONTAINER_OF(work, struct paw3232_data,
						  init_work.work);
	 const struct device *dev = data->dev;
 
	 LOG_DBG("PAW3232 async init step %d", data->async_init_step);
 
	 data->err = async_init_fn[data->async_init_step](dev);
	 if (data->err) {
		 LOG_ERR("PAW3232 initialization failed");
	 } else {
		 data->async_init_step++;
 
		 if (data->async_init_step == ASYNC_INIT_STEP_COUNT) {
			 data->ready = true;
			 LOG_INF("PAW3232 initialized");
		 } else {
			 k_work_schedule(&data->init_work,
					 K_MSEC(async_init_delay[
						 data->async_init_step]));
		 }
	 }
 }
 
 static int paw3232_init_irq(const struct device *dev)
 {
	 int err;
	 struct paw3232_data *data = dev->data;
	 const struct paw3232_config *config = dev->config;
 
	 if (!device_is_ready(config->irq_gpio.port)) {
		 LOG_ERR("IRQ GPIO device not ready");
		 return -ENODEV;
	 }
 
	 err = gpio_pin_configure_dt(&config->irq_gpio, GPIO_INPUT);
	 if (err) {
		 LOG_ERR("Cannot configure IRQ GPIO");
		 return err;
	 }
 
	 gpio_init_callback(&data->irq_gpio_cb, irq_handler,
				BIT(config->irq_gpio.pin));
 
	 err = gpio_add_callback(config->irq_gpio.port,
				 &data->irq_gpio_cb);
 
	 if (err) {
		 LOG_ERR("Cannot add IRQ GPIO callback");
	 }
 
	 return err;
 }
 
 static int paw3232_init(const struct device *dev)
 {
	 struct paw3232_data *data = dev->data;
	 const struct paw3232_config *config = dev->config;
	 int err;
 
 
	 k_work_init(&data->trigger_handler_work, trigger_handler);
	 data->dev = dev;
 
	 if (!spi_is_ready_dt(&config->bus)) {
		 return -ENODEV;
	 }
 
	 if (!device_is_ready(config->cs_gpio.port)) {
		 LOG_ERR("SPI CS device not ready");
		 return -ENODEV;
	 }
 
	 err = gpio_pin_configure_dt(&config->cs_gpio, GPIO_OUTPUT_INACTIVE);
	 if (err) {
		 LOG_ERR("Cannot configure SPI CS GPIO");
		 return err;
	 }
 
	 err = paw3232_init_irq(dev);
	 if (err) {
		 return err;
	 }
 
	 k_work_init_delayable(&data->init_work, paw3232_async_init);
 
	 k_work_schedule(&data->init_work,
			 K_MSEC(async_init_delay[data->async_init_step]));
 
	 return err;
 }
 
 static int paw3232_sample_fetch(const struct device *dev, enum sensor_channel chan)
 {
	 struct paw3232_data *data = dev->data;
	 uint8_t motion_status;
	 int err;
 
	 if (unlikely(chan != SENSOR_CHAN_ALL)) {
		 return -ENOTSUP;
	 }
 
	 if (unlikely(!data->ready)) {
		 LOG_INF("Device is not initialized yet");
		 return -EBUSY;
	 }
 
	 err = reg_read(dev, PAW3232_REG_MOTION, &motion_status);
	 if (err) {
		 LOG_ERR("Cannot read motion");
		 return err;
	 }
 
	 if ((motion_status & PAW3232_MOTION_STATUS_MOTION) != 0) {
		 uint8_t x_low;
		 uint8_t y_low;
 
		 if ((motion_status & PAW3232_MOTION_STATUS_DXOVF) != 0) {
			 LOG_WRN("X delta overflowed");
		 }
		 if ((motion_status & PAW3232_MOTION_STATUS_DYOVF) != 0) {
			 LOG_WRN("Y delta overflowed");
		 }
 
		 err = reg_read(dev, PAW3232_REG_DELTA_X_LOW, &x_low);
		 if (err) {
			 LOG_ERR("Cannot read X delta");
			 return err;
		 }
 
		 err = reg_read(dev, PAW3232_REG_DELTA_Y_LOW, &y_low);
		 if (err) {
			 LOG_ERR("Cannot read Y delta");
			 return err;
		 }

		 if (IS_ENABLED(CONFIG_PAW3232_16_BIT_MODE)) {
			uint8_t x_high;
			uint8_t y_high;
			err = reg_read(dev, PAW3232_REG_DELTA_X_HIGH,
				       &x_high);
			if (err) {
				LOG_ERR("Cannot read XY delta high");
				return err;
			}
			err = reg_read(dev, PAW3232_REG_DELTA_Y_HIGH,
				       &y_high);
			if (err) {
				LOG_ERR("Cannot read XY delta high");
				return err;
			}
			data->x = PAW3232_DELTA_X(x_high, x_low);
			data->y = PAW3232_DELTA_Y(y_high, y_low);
		 }

		 else {
			 data->x = (int8_t)x_low;
			 data->y = (int8_t)y_low;
		 }
	 } else {
		 data->x = 0;
		 data->y = 0;
	 }
 #if 0 //for debug
	 int x,y;
	 x=data->x;
	 y=data->y*-1;
	 uint8_t x_raw[4],y_raw[4];
	 memcpy(x_raw,&x,4);
	 memcpy(y_raw,&y,4);
	 LOG_ERR("%02x %02x %02x %02x %02x %02x %02x %02x\n",x_raw[0], x_raw[1],x_raw[2], x_raw[3],y_raw[0], y_raw[1],y_raw[2], y_raw[3]);
	 //LOG_ERR("%d,%d",x, y);
	 //LOG_ERR("0x%08x,0x%08x",x, y);
	 //LOG_ERR("darfon x: %d,y = %d",data->x, data->y);
 #endif
	 return err;
 }
 
 static int paw3232_channel_get(const struct device *dev, enum sensor_channel chan,
					struct sensor_value *val)
 {
	 struct paw3232_data *data = dev->data;
 
	 if (unlikely(!data->ready)) {
		 LOG_INF("Device is not initialized yet");
		 return -EBUSY;
	 }
 
	 switch (chan) {
	 case SENSOR_CHAN_POS_DX:
		 val->val1 = data->x;
		 val->val2 = 0;
		 break;
 
	 case SENSOR_CHAN_POS_DY:
		 val->val1 = data->y;
		 val->val2 = 0;
		 break;
 
	 default:
		 return -ENOTSUP;
	 }
 
	 return 0;
 }
 
 static int paw3232_trigger_set(const struct device *dev,
					const struct sensor_trigger *trig,
					sensor_trigger_handler_t handler)
 {
	 struct paw3232_data *data = dev->data;
	 const struct paw3232_config *config = dev->config;
	 int err;
 
	 if (unlikely(trig->type != SENSOR_TRIG_DATA_READY)) {
		 return -ENOTSUP;
	 }
 
	 if (unlikely(trig->chan != SENSOR_CHAN_ALL)) {
		 return -ENOTSUP;
	 }
 
	 if (unlikely(!data->ready)) {
		 LOG_INF("Device is not initialized yet");
		 return -EBUSY;
	 }
 
	 k_spinlock_key_t key = k_spin_lock(&data->lock);
 
	 if (handler) {
		 err = gpio_pin_interrupt_configure_dt(&config->irq_gpio,
							   GPIO_INT_LEVEL_ACTIVE);
	 } else {
		 err = gpio_pin_interrupt_configure_dt(&config->irq_gpio,
							   GPIO_INT_DISABLE);
	 }
 
	 if (!err) {
		 data->data_ready_handler = handler;
	 }
 
	 k_spin_unlock(&data->lock, key);
 
	 return err;
 }
 
 static int paw3232_attr_set(const struct device *dev, enum sensor_channel chan,
				 enum sensor_attribute attr,
				 const struct sensor_value *val)
 {
	 struct paw3232_data *data = dev->data;
	 int err;
 
	 if (unlikely(chan != SENSOR_CHAN_ALL)) {
		 return -ENOTSUP;
	 }
 
	 if (unlikely(!data->ready)) {
		 LOG_INF("Device is not initialized yet");
		 return -EBUSY;
	 }
	 switch ((uint32_t)attr) {
	 case PAW3232_ATTR_CPI:
		 err = update_cpi(dev, PAW3232_SVALUE_TO_CPI(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP_ENABLE:
		 err = toggle_sleep_modes(dev,
					  PAW3232_REG_OPERATION_MODE,
					  PAW3232_REG_CONFIGURATION,
					  PAW3232_SVALUE_TO_BOOL(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP1_TIMEOUT:
		 err = update_sleep_timeout(dev,
						PAW3232_REG_SLEEP1,
						PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP2_TIMEOUT:
		 err = update_sleep_timeout(dev,
						PAW3232_REG_SLEEP2,
						PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP3_TIMEOUT:
		 err = update_sleep_timeout(dev,
						PAW3232_REG_SLEEP3,
						PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP1_SAMPLE_TIME:
		 err = update_sample_time(dev,
					  PAW3232_REG_SLEEP1,
					  PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP2_SAMPLE_TIME:
		 err = update_sample_time(dev,
					  PAW3232_REG_SLEEP2,
					  PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 case PAW3232_ATTR_SLEEP3_SAMPLE_TIME:
		 err = update_sample_time(dev,
					  PAW3232_REG_SLEEP3,
					  PAW3232_SVALUE_TO_TIME(*val));
		 break;
 
	 default:
		 LOG_ERR("Unknown attribute");
		 return -ENOTSUP;
	 }
	 return err;
 }
 
 static const struct sensor_driver_api paw3232_driver_api = {
	 .sample_fetch = paw3232_sample_fetch,
	 .channel_get  = paw3232_channel_get,
	 .trigger_set  = paw3232_trigger_set,
	 .attr_set     = paw3232_attr_set,
 };
 
 #define PAW3232_DEFINE(n)						       \
	 static struct paw3232_data data##n;				       \
											\
	 static const struct paw3232_config config##n = {		       \
		 .irq_gpio = GPIO_DT_SPEC_INST_GET(n, irq_gpios),	       \
		 .bus = {						       \
			 .bus = DEVICE_DT_GET(DT_INST_BUS(n)),		       \
			 .config = {					       \
				 .frequency = DT_INST_PROP(n,		       \
							   spi_max_frequency),  \
				 .operation = SPI_WORD_SET(8) |		       \
						  SPI_TRANSFER_MSB |		       \
						  SPI_MODE_CPOL | SPI_MODE_CPHA,    \
				 .slave = DT_INST_REG_ADDR(n),		       \
			 },						       \
		 },							       \
		 .cs_gpio = SPI_CS_GPIOS_DT_SPEC_GET(DT_DRV_INST(n)),	       \
	 };								       \
											\
	 DEVICE_DT_INST_DEFINE(n, paw3232_init, NULL, &data##n, &config##n,     \
				   POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,	       \
				   &paw3232_driver_api);
 
 DT_INST_FOREACH_STATUS_OKAY(PAW3232_DEFINE)
 