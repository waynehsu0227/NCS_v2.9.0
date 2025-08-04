#ifndef ZEPHYR_DRIVERS_CHARGER_BQ2561X_H_
#define ZEPHYR_DRIVERS_CHARGER_BQ2561X_H_

/*REG00*/
#define BQ2561X_INPUT_CURRENT_LIMIT			0x00
#define BQ2561X_EN_HIZ_MASK		            BIT(7)
#define BQ2561X_TS_IGNORE_MASK		        BIT(6)
#define BQ2561X_BATSNS_DIS_MASK		        BIT(5)
/* Input current limit setting */
#define BQ2561X_IINDPM_DEFAULT_uA			2400000		// 2400mA
#define BQ2561X_IINDPM_MAX_uA				3200000		// 3200mA
#define BQ2561X_IINDPM_MIN_uA				100000		// 100mA
#define BQ2561X_IINDPM_STEP_uA				100000		// 100mA
#define BQ2561X_IINDPM_MASK		            GENMASK(4, 0)

/*REG01*/
#define BQ2561X_CHARGER_CONTROL_0			0x01
#define BQ2561X_CHG_CONFIG_MASK				BIT(4)
#define BQ2561X_SYS_MIN_DEFAULT_mv          3500
#define BQ2561X_SYS_MIN_MAX_mv              3700
#define BQ2561X_SYS_MIN_MIN_mv              2600
#define BQ2561X_SYS_MIN_STEP_mv             100
#define BQ2561X_SYS_MIN_3V                  2
#define BQ2561X_SYS_MIN_MASK				GENMASK(3, 1)


/*REG02*/
#define BQ2561X_CHARGE_CURRENT_LIMIT		0x02
/* Fast charge current setting */
#define BQ2561X_ICHG_DEFAULT_uA				340000		// 340mA
#define BQ2561X_ICHG_MAX_uA					1180000		// 1180mA
#define BQ2561X_ICHG_MIN_uA					0			// 0mA
#define BQ2561X_ICHG_STEP_uA				200000		// 20mA
#define BQ2561X_ICHG_SPEC_VAL_uA			1500000,1430000,1360000,1290000,BQ2561X_ICHG_MAX_uA
#define BQ2561X_ICHG_VAL_MAX		        0x3F
#define BQ2561X_ICHG_MASK		            GENMASK(5, 0)

/*REG03*/
#define BQ2561X_PRECHG_AND_TERM_CURR_LIM	0x03
/* Precharge current setting */
#define BQ2561X_IPRECHG_DEFAULT_uA			40000		// 40mA
#define BQ2561X_IPRECHG_MAX_uA				260000		// 260mA
#define BQ2561X_IPRECHG_MIN_uA				20000		// 20mA
#define BQ2561X_IPRECHG_STEP_uA				20000		// 20mA
#define BQ2561X_IPRECHG_MASK		        GENMASK(7, 4)
/* Termination current setting */
#define BQ2561X_ITERM_DEFAULT_uA			60000		// 60mA
#define BQ2561X_ITERM_MAX_uA				260000		// 260mA
#define BQ2561X_ITERM_MIN_uA				20000		// 20mA
#define BQ2561X_ITERM_STEP_uA				20000		// 20mA
#define BQ2561X_ITERM_MASK		            GENMASK(3, 0)

/*REG04*/
#define BQ2561X_BATTERY_VOLTAGE_LIMIT		0x04
/* Battery voltage setting */
#define BQ2561X_VBATREG_DEFAULT_mV			4200		//Battery voltage setting (4200mV)
#define BQ2561X_VBATREG_MAX_mV				4520
#define BQ2561X_VBATREG_MIN_mV				4300
#define BQ2561X_VBATREG_STEP_mV				10
#define BQ2561X_VBATREG_SPEC_VAL_mV			3504,3600,3696,3800,3904,4000,4100,4150,4200
#define BQ2561X_VBATREG_MASK				GENMASK(7, 3)

/*REG05*/
#define BQ2561X_CHARGER_CONTROL_1		    0x05
#define BQ2561X_EN_TERM                     BIT(7)
#define BQ2561X_WATCHDOG_DIS                0
#define BQ2561X_WATCHDOG_40S                BIT(4)
#define BQ2561X_WATCHDOG_80S                BIT(5)
#define BQ2561X_WATCHDOG_160S               BIT(5) | BIT(4)
#define BQ2561X_WATCHDOG_MASK               GENMASK(5, 4)
#define BQ2561X_EN_TIMER                    BIT(3)
#define BQ2561X_CHG_TIMER                   BIT(2)
#define BQ2561X_TREG                        BIT(1)
#define BQ2561X_JEITA_VSET                  BIT(0)

/*REG06*/
#define BQ2561X_CHARGER_CONTROL_2		    0x06
/* VINDPM threshold setting */
#define BQ2561X_VINDPM_DEFAULT_mV			4500		// 4500mV
#define BQ2561X_VINDPM_MAX_mV				5400		// 5400mV
#define BQ2561X_VINDPM_MIN_mV	            3900		// 3900mV
#define BQ2561X_VINDPM_STEP_mV			    100			// 100mA
#define BQ2561X_VINDPM_MASK				    GENMASK(3, 0)

/*REG07*/
#define BQ2561X_CHARGER_CONTROL_3		    0x07
#define BQ2561X_IINDET_EN                   BIT(7)
#define BQ2561X_TMR2X_EN                    BIT(6)
#define BQ2561X_BATFET_DIS                  BIT(5)
#define BQ2561X_BATFET_RST_WVBUS            BIT(4)
#define BQ2561X_BATFET_DLY                  BIT(3)
#define BQ2561X_BATFET_RST_EN               BIT(2)
#define BQ2561X_VINDPM_BAT_TRACK_DISABLE    0
#define BQ2561X_VINDPM_BAT_TRACK_P_200mV    BIT(0)
#define BQ2561X_VINDPM_BAT_TRACK_P_250mV    BIT(1)
#define BQ2561X_VINDPM_BAT_TRACK_P_300mV    BIT(1) | BIT(0)
#define BQ2561X_VINDPM_BAT_TRACK_MASK		GENMASK(1, 0)

/*REG08*/
#define BQ2561X_CHARGER_STATUS_0		    0x08
#define BQ2561X_CHRG_STAT_NO_INPUT          0
#define BQ2561X_CHRG_STAT_USB_HOST          1// BIT(5)
#define BQ2561X_CHRG_STAT_ADAPTER           3//BIT(6) | BIT(5)
#define BQ2561X_CHRG_STAT_BOOST_MODE        7// BIT(7) | BIT(6) | BIT(5)
#define BQ2561X_VBUS_STAT_MASK		        GENMASK(7, 5)
#define BQ2561X_CHRG_STAT_NOT_CHRGING	    0
#define BQ2561X_CHRG_STAT_PRECHRGING	    1// BIT(3)
#define BQ2561X_CHRG_STAT_FAST_CHRGING	    2// BIT(4)
#define BQ2561X_CHRG_STAT_CHRG_TERM	        3//BIT(4) | BIT(3)
#define BQ2561X_CHRG_STAT_MASK		        GENMASK(4, 3)
#define BQ2561X_PG_STAT                     BIT(2)
#define BQ2561X_THERM_STAT                  BIT(1)
#define BQ2561X_VSYS_STAT                   BIT(0)

/*REG09*/
#define BQ2561X_CHARGER_STATUS_1		    0x09
#define BQ2561X_WATCHDOG_FAULT              BIT(7)
#define BQ2561X_BOOST_FAULT                 BIT(6)
#define BQ2561X_CHRG_FAULT_NORMAL           0
#define BQ2561X_CHRG_FAULT_INPUT            1//BIT(4)
#define BQ2561X_CHRG_FAULT_THERMAL          2// BIT(5)
#define BQ2561X_CHRG_FAULT_TIMER            3//BIT(5) | BIT(4)
#define BQ2561X_CHRG_FAULT_MASK		        GENMASK(5, 4)
#define BQ2561X_BAT_FAULT                   BIT(3)
#define BQ2561X_NTC_FAULT_NORMAL            0
#define BQ2561X_NTC_FAULT_WARM              2
#define BQ2561X_NTC_FAULT_COOL              3
#define BQ2561X_NTC_FAULT_COLD              5
#define BQ2561X_NTC_FAULT_HOT               6
#define BQ2561X_NTC_FAULT_MASK		        GENMASK(2, 0)

/*REG0A*/
#define BQ2561X_CHARGER_STATUS_2		    0x0A
#define BQ2561X_VBUS_GD				        BIT(7)
#define BQ2561X_VINDPM_STAT				    BIT(6)
#define BQ2561X_IINDPM_STAT				    BIT(5)
#define BQ2561X_BATSNS_STAT				    BIT(4)
#define BQ2561X_TOPOFF_ACTIVE				BIT(3)
#define BQ2561X_ACOV_STAT				    BIT(2)
#define BQ2561X_VINDPM_INT_MASK				BIT(1)
#define BQ2561X_IINDPM_INT_MASK			    BIT(0)

/*REG0B*/
#define BQ2561X_PART_INFORMATION		    0x0B
#define BQ2561X_RESET_MASK				    BIT(7)
#define BQ2561X_RESET_MAX_TRIES			    100

/*REG0C*/
#define BQ2561X_CHARGER_CONTROL_4		    0x0C
#define BQ2561X_JEITA_COOL_ISET_NO          0
#define BQ2561X_JEITA_COOL_ISET_20          BIT(6)
#define BQ2561X_JEITA_COOL_ISET_50          BIT(7)
#define BQ2561X_JEITA_COOL_ISET_100         BIT(7) | BIT(6)
#define BQ2561X_JEITA_COOL_ISET_MASK        GENMASK(7, 6)
#define BQ2561X_JEITA_WARM_ISET_NO          0
#define BQ2561X_JEITA_WARM_ISET_20          BIT(4)
#define BQ2561X_JEITA_WARM_ISET_50          BIT(5)
#define BQ2561X_JEITA_WARM_ISET_100         BIT(5) | BIT(4)
#define BQ2561X_JEITA_WARM_ISET_MASK        GENMASK(5, 4)
#define BQ2561X_JEITA_VT2_5_5               0
#define BQ2561X_JEITA_VT2_10                BIT(2)
#define BQ2561X_JEITA_VT2_15                BIT(3)
#define BQ2561X_JEITA_VT2_20                BIT(3) | BIT(2)
#define BQ2561X_JEITA_VT2_MASK              GENMASK(3, 2)
#define BQ2561X_JEITA_VT3_40                0
#define BQ2561X_JEITA_VT3_44_5              BIT(0)
#define BQ2561X_JEITA_VT3_50_5              BIT(1) 
#define BQ2561X_JEITA_VT3_54_5              BIT(1) | BIT(0)
#define BQ2561X_JEITA_VT3_MASK              GENMASK(1, 0)

#endif /*  ZEPHYR_DRIVERS_CHARGER_BQ2561X_H_  */