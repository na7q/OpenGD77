/*
 * Copyright (C)2019 Kai Ludwig, DG4KLU
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _FW_WDOG_H_
#define _FW_WDOG_H_

#include "fsl_wdog.h"

#include "fw_adc.h"

typedef void (*batteryHistoryCallback_t)(int32_t);

extern volatile bool alive_maintask;
extern volatile bool alive_beeptask;
extern volatile bool alive_hrc6000task;

extern int battery_voltage;
extern int battery_voltage_tick;
extern float averageBatteryVoltage;
extern bool batteryVoltageHasChanged;

void init_watchdog(batteryHistoryCallback_t cb);
void fw_watchdog_task(void *data);
void tick_watchdog(void);
void watchdogReboot(void);

#endif /* _FW_WDOG_H_ */
