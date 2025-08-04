#ifndef ZEPHYR_DRIVERS_CHARGER_BT2306_H_
#define ZEPHYR_DRIVERS_CHARGER_BT2306_H_
#include <zephyr/drivers/sensor.h>
/*REG00*/
#define BT2306_INPUT_SOURCE_CONTROL			0x00
#define BT2306_EN_HIZ_MASK		            BIT(7)
#define BT2306_VINDPM_MASK		            GENMASK(6, 3)
#define BT2306_IINLIM_MASK		            GENMASK(2, 0)

#define BT2306_VINDPM_640mv                BIT(3)
#define BT2306_VINDPM_320mv                BIT(2)
#define BT2306_VINDPM_160mv                BIT(1)
#define BT2306_VINDPM_80mv                 BIT(0)
#define BT2306_IINLIM_100ma                0
#define BT2306_IINLIM_150ma                0x1
#define BT2306_IINLIM_500ma                0x2
#define BT2306_IINLIM_900ma                0x3
#define BT2306_IINLIM_1a                   0x4
#define BT2306_IINLIM_1d5a                 0x5
#define BT2306_IINLIM_2a                   0x6
#define BT2306_IINLIM_3a                   0x7

/*REG01*/
#define BT2306_POWER_ON_CONFIG 			0x01
#define BT2306_REG_RESET_MASK       			BIT(7)
#define BT2306_I2C_WTD_RESET_MASK      			BIT(6) 
#define BT2306_OTG_CONFIG_MASK      			BIT(5) 
#define BT2306_CHG_CONFIG_MASK      			BIT(4) 
#define BT2306_SYS_MIN_MASK				GENMASK(3, 1)
#define BT2306_SYS_MIN_0d4v                 BIT(2)
#define BT2306_SYS_MIN_0d2v                 BIT(1)
#define BT2306_SYS_MIN_0d1v                 BIT(0)
#define BT2306_SYS_MIN_DEF_3V               0


/*REG02*/
#define BT2306_CHARGE_CURRENT_CONTROL		0x02
/* Fast charge current setting */
#define BT2306_ICHG_MASK		            GENMASK(7, 2)
#define BT2306_BCOLD_MASK           			BIT(1) 
#define BT2306_FORCE_20PCT_MASK      			BIT(0) 
#define BT2306_ICHG_2048ma 		        BIT(5)
#define BT2306_ICHG_1024ma 		        BIT(4)
#define BT2306_ICHG_512ma 		            BIT(3)
#define BT2306_ICHG_256ma 		            BIT(2)
#define BT2306_ICHG_128ma 		            BIT(1)
#define BT2306_ICHG_64ma 		            BIT(0)
#define BT2306_ICHG_BASE_512ma 		            0

/*REG03*/
#define BT2306_PRECHG_TERM_CURR_LIM	0x03
#define BT2306_IPRECHG_MASK		        GENMASK(7, 4)
#define BT2306_ITERM_MASK		            GENMASK(3, 0)
/* Precharge current setting */
#define BT2306_IPRECHG_1024ma      		BIT(3)
#define BT2306_IPRECHG_512ma      		    BIT(2)
#define BT2306_IPRECHG_256ma      		    BIT(1)
#define BT2306_IPRECHG_128ma      		    BIT(0)

/* Termination current setting */
#define BT2306_ITERM_1024ma      		    BIT(3)
#define BT2306_ITERM_512ma      		    BIT(2)
#define BT2306_ITERM_256ma      		    BIT(1)
#define BT2306_ITERM_128ma      		    BIT(0)

/*REG04*/
#define BT2306_CHARGE_VOLTAGE_CONTROL		0x04
#define BT2306_VREG_MASK				GENMASK(7, 2)
#define BT2306_BATLOWV_MASK                     BIT(1)
#define BT2306_VREGCHG_MASK                     BIT(0)

/* charge voltage setting */
#define BT2306_VREG_512mv      			BIT(5)
#define BT2306_VREG_256mv      			BIT(4)
#define BT2306_VREG_128mv      			BIT(3)
#define BT2306_VREG_64mv      			    BIT(2)
#define BT2306_VREG_32mv      			    BIT(1)
#define BT2306_VREG_16mv      			    BIT(0)
#define BT2306_VREG_BASE_3504mv      	    0


/*REG05*/
#define BT2306_CHARGER_TERM_TIMER_CONTROL		    0x05
#define BT2306_EN_TERM_MASK                     BIT(7)
#define BT2306_WATCHDOG_MASK               GENMASK(5, 4)
#define BT2306_EN_TIMER_MASK                    BIT(3)
#define BT2306_CHG_TIMER_MASK               GENMASK(2, 1)

#define BT2306_WATCHDOG_160s               0x3
#define BT2306_WATCHDOG_80s                0x2
#define BT2306_WATCHDOG_40s                0x1
#define BT2306_WATCHDOG_DISABLE            0
#define BT2306_CHG_TIMER_20hrs             0x3
#define BT2306_CHG_TIMER_12hrs             0x2
#define BT2306_CHG_TIMER_8hrs              0x1
#define BT2306_CHG_TIMER_5hrs              0

/*REG06*/
#define BT2306_BOOST_VOLTAGE_THERMAL_REGULATION_CONTROL		    0x06
#define BT2306_BOOSTV_MASK				    GENMASK(7, 4)
#define BT2306_BHOT_MASK				    GENMASK(3, 2)
#define BT2306_TREG_MASK				    GENMASK(1, 0)

#define BT2306_BOOSTV_512mv                BIT(3)
#define BT2306_BOOSTV_256mv                BIT(2)
#define BT2306_BOOSTV_128mv                BIT(1)
#define BT2306_BOOSTV_64mv                 BIT(0)
#define BT2306_TREG_120C                   0x3
#define BT2306_TREG_100C                   0x2
#define BT2306_TREG_80C                    0x1
#define BT2306_TREG_60C                    0

/*REG07*/
#define BT2306_MISC_OPERATION_CONTROL		 0x07
#define BT2306_DPM_EN_MASK                  BIT(7)
#define BT2306_TMR2X_EN_MASK                BIT(6)
#define BT2306_BATFET_DISABLE_MASK          BIT(5)
#define BT2306_INT_MASK_MASK                GENMASK(1, 0)


 

/*REG08*/
#define BT2306_SYSTEM_STATUS		        0x08
#define BT2306_VBUS_STAT_MASK              GENMASK(7, 6)
#define BT2306_CHRG_STAT_MASK              GENMASK(5, 4)
#define BT2306_DPM_STAT                    BIT(3)
#define BT2306_PG_STAT                     BIT(2)
#define BT2306_THERM_STAT                  BIT(1)
#define BT2306_VSYS_STAT                   BIT(0)

#define BT2306_VBUS_STAT_NO_INPUT          0
#define BT2306_VBUS_STAT_USB_HOST          1
#define BT2306_VBUS_STAT_ADAPTER           2
#define BT2306_VBUS_STAT_OTG_MODE          3

#define BT2306_CHRG_STAT_NOT_CHRGING	    0
#define BT2306_CHRG_STAT_PRECHRGING	    1
#define BT2306_CHRG_STAT_FAST_CHRGING	    2
#define BT2306_CHRG_STAT_CHRG_TERM	        3

/*REG09*/
#define BT2306_NEW_FAULT          		    0x09
#define BT2306_WATCHDOG_FAULT_MASK	        BIT(7)
#define BT2306_OTG_FAULT_MASK	            BIT(6)
#define BT2306_CHRG_FAULT_MASK		        GENMASK(5, 4)
#define BT2306_BAT_FAULT_MASK	            BIT(3)
#define BT2306_NTC_FAULT_MASK		        GENMASK(1, 0)

#define BT2306_CHRG_FAULT_NORMAL           0
#define BT2306_CHRG_FAULT_INPUT            1//BIT(4)
#define BT2306_CHRG_FAULT_THERMAL          2// BIT(5)
#define BT2306_CHRG_FAULT_TIMER            3//BIT(5) | BIT(4)
#define BT2306_NTC_FAULT_COLD              2
#define BT2306_NTC_FAULT_HOT               1




/*REG0A*/
#define BT2306_VENDER_PART_REVISION_STATUS		    0x0A
#define BT2306_PN_MASK				        GENMASK(7, 5)
#define BT2306_REV_MASK				    GENMASK(2, 0)



#define BT2306_RESET_MAX_TRIES			    100



/*USER DEFINE*/
 

#endif /*  ZEPHYR_DRIVERS_CHARGER_BT2306_H_  */