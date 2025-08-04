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

#define BATTERY_MEAS_VOLTAGE_GAIN	4/2

/* Converting measured battery voltage[mV] to State of Charge[%].
 * First element corresponds to CONFIG_DESKTOP_BATTERY_MIN_LEVEL.
 * Each element is CONFIG_DESKTOP_VOLTAGE_TO_SOC_DELTA higher than previous.
 * Defined separately for every configuration.
 */

#if (CONFIG_CY25)
static const uint16_t battery_table[101] = {
    /* 0*/  CONFIG_DESKTOP_BATTERY_MEAS_MIN_LEVEL, /* 1*/  962, /* 2*/  973, /* 3*/  985, /* 4*/  997, /* 5*/ 1008, /* 6*/ 1020, /* 7*/ 1032, /* 8*/ 1043, /* 9*/ 1055,
    /*10*/ 1067, /*11*/ 1078, /*12*/ 1090, /*13*/ 1102, /*14*/ 1113, /*15*/ 1125, /*16*/ 1137, /*17*/ 1148, /*18*/ 1160, /*19*/ 1172,
    /*20*/ 1183, /*21*/ 1195, /*22*/ 1207, /*23*/ 1218, /*24*/ 1230, /*25*/ 1240, /*26*/ 1244, /*27*/ 1247, /*28*/ 1250, /*29*/ 1253,
    /*30*/ 1257, /*31*/ 1260, /*32*/ 1263, /*33*/ 1266, /*34*/ 1270, /*35*/ 1273, /*36*/ 1276, /*37*/ 1279, /*38*/ 1283, /*39*/ 1286,
    /*40*/ 1289, /*41*/ 1292, /*42*/ 1296, /*43*/ 1299, /*44*/ 1302, /*45*/ 1305, /*46*/ 1309, /*47*/ 1312, /*48*/ 1315, /*49*/ 1318,
    /*50*/ 1322, /*51*/ 1325, /*52*/ 1328, /*53*/ 1331, /*54*/ 1335, /*55*/ 1338, /*56*/ 1341, /*57*/ 1344, /*58*/ 1347, /*59*/ 1351,
    /*60*/ 1354, /*61*/ 1357, /*62*/ 1360, /*63*/ 1364, /*64*/ 1367, /*65*/ 1370, /*66*/ 1373, /*67*/ 1377, /*68*/ 1380, /*69*/ 1383,
    /*70*/ 1386, /*71*/ 1390, /*72*/ 1393, /*73*/ 1396, /*74*/ 1399, /*75*/ 1403, /*76*/ 1406, /*77*/ 1409, /*78*/ 1412, /*79*/ 1416,
    /*80*/ 1419, /*81*/ 1422, /*82*/ 1425, /*83*/ 1429, /*84*/ 1432, /*85*/ 1435, /*86*/ 1438, /*87*/ 1442, /*88*/ 1445, /*89*/ 1448,
    /*90*/ 1451, /*91*/ 1455, /*92*/ 1458, /*93*/ 1462, /*94*/ 1469, /*95*/ 1476, /*96*/ 1483, /*97*/ 1490, /*98*/ 1492, /*99*/ 1498,
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