/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* This configuration file is included only once from battery level measuring
 * module and holds information about battery characteristic.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} battery_def_include_once;

#define BATTERY_MEAS_VOLTAGE_GAIN	3/1

/* Converting measured battery voltage[mV] to State of Charge[%].
 * First element corresponds to CONFIG_DESKTOP_BATTERY_MIN_LEVEL.
 * Each element is CONFIG_DESKTOP_VOLTAGE_TO_SOC_DELTA higher than previous.
 * Defined separately for every configuration.
 */
#if (CONFIG_CY25)
    uint16_t battery_table[101] = {
    /* 0*/ CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL,/* 1*/1924,/* 2*/1946,/* 3*/1970,/* 4*/1994,/* 5*/2016,/* 6*/2040,/* 7*/2064,/* 8*/2086,/* 9*/2110,
    /*10*/ 2134,/*11*/2156,/*12*/2180,/*13*/2204,/*14*/2226,/*15*/2250,/*16*/2274,/*17*/2296,/*18*/2320,/*19*/2344,
    /*20*/ 2366,/*21*/2390,/*22*/2414,/*23*/2436,/*24*/2460,/*25*/2480,/*26*/2488,/*27*/2494,/*28*/2500,/*29*/2506,
    /*30*/ 2514,/*31*/2520,/*32*/2526,/*33*/2532,/*34*/2540,/*35*/2546,/*36*/2552,/*37*/2558,/*38*/2566,/*39*/2572,
    /*40*/ 2578,/*41*/2584,/*42*/2592,/*43*/2598,/*44*/2604,/*45*/2610,/*46*/2618,/*47*/2624,/*48*/2630,/*49*/2636,
    /*50*/ 2644,/*51*/2650,/*52*/2656,/*53*/2662,/*54*/2670,/*55*/2676,/*56*/2682,/*57*/2688,/*58*/2694,/*59*/2702,
    /*60*/ 2708,/*61*/2714,/*62*/2720,/*63*/2728,/*64*/2734,/*65*/2740,/*66*/2746,/*67*/2754,/*68*/2760,/*69*/2766,
    /*70*/ 2772,/*71*/2780,/*72*/2786,/*73*/2792,/*74*/2798,/*75*/2806,/*76*/2812,/*77*/2818,/*78*/2824,/*79*/2832,
    /*80*/ 2838,/*81*/2844,/*82*/2850,/*83*/2858,/*84*/2864,/*85*/2870,/*86*/2876,/*87*/2884,/*88*/2890,/*89*/2896,
    /*90*/ 2902,/*91*/2910,/*92*/2916,/*93*/2924,/*94*/2938,/*95*/2952,/*96*/2966,/*97*/2980,/*98*/2984,/*99*/2996,
    /*100*/CONFIG_DESKTOP_BATTERY_MEAS_MAX_LEVEL
    };
#else
static const uint8_t battery_voltage_to_soc[] = {
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
 1,  1,  2,  2,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,  6,  7,  8,  9,  9, 10,
11, 13, 14, 15, 16, 18, 19, 21, 23, 24, 26, 28, 31, 33, 35, 37, 40, 42, 45, 47,
50, 52, 54, 57, 59, 62, 64, 66, 68, 71, 73, 75, 76, 78, 80, 81, 83, 84, 85, 86,
88, 89, 90, 90, 91, 92, 93, 93, 94, 94, 95, 95, 96, 96, 96, 97, 97, 97, 97, 98,
98, 98, 98, 98, 98, 98, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 100,
100
};
#endif
