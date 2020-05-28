/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
 * 				and	 Colin Durbridge, G4EML
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

#include <codeplug.h>
#include <settings.h>
#include <trx.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiUtilities.h>
#include <user_interface/uiLocalisation.h>

static void updateScreen(void);
static void updateCursor(bool moved);
static void handleEvent(uiEvent_t *ev);

static int32_t RxCSSIndex = 0;
static int32_t TxCSSIndex = 0;
typedef enum {
	CSS_NONE = 0,
	CSS_CTCSS,
	CSS_DCS
} CSSTypes_t;
static CSSTypes_t RxCSSType = CSS_NONE;
static CSSTypes_t TxCSSType = CSS_NONE;
static struct_codeplugChannel_t tmpChannel;// update a temporary copy of the channel and only write back if green menu is pressed
static char channelName[17];
static int namePos;


enum CHANNEL_DETAILS_DISPLAY_LIST {  CH_DETAILS_RXCTCSS = 0, 
									 CH_DETAILS_TXCTCSS, CH_DETAILS_BANDWIDTH, 
									 CH_DETAILS_FREQ_STEP, CH_DETAILS_MODE, 
									 CH_DETAILS_RXGROUP, CH_DETAILS_DMR_CC,
									 CH_DETAILS_DMR_TS, CH_DETAILS_ZONE_SKIP,
									 CH_DETAILS_ALL_SKIP, CH_DETAILS_TOT,
									 CH_DETAILS_NAME, CH_DETAILS_RXFREQ,
									 CH_DETAILS_TXFREQ, CH_DETAILS_VOX,
									NUM_CH_DETAILS_ITEMS};// The last item in the list is used so that we automatically get a total number of items in the list

// Returns the index in either the CTCSS or DCS list of the tone (or closest match)
static int cssIndex(uint16_t tone, CSSTypes_t type)
{
	if (type == CSS_DCS)
	{
		tone &= 0777;
		for (int i = 0; i < TRX_NUM_DCS; i++)
		{
			if (TRX_DCSCodes[i] >= tone)
			{
				return i;
			}
		}
	}
	else if (type == CSS_CTCSS)
	{
		for (int i = 0; i < TRX_NUM_CTCSS; i++)
		{
			if (TRX_CTCSSTones[i] >= tone)
			{
				return i;
			}
		}
	}
	return 0;
}

static void cssIncrement(uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	(*index)++;
	if (*type == CSS_CTCSS)
	{
		if (*index >= TRX_NUM_CTCSS)
		{
			*type = CSS_DCS;
			*index = 0;
			*tone = TRX_DCSCodes[*index] | 0x8000;
			return;
		}
		*tone = TRX_CTCSSTones[*index];
	}
	else if (*type == CSS_DCS)
	{
		if (*index >= TRX_NUM_DCS)
		{
			*index = TRX_NUM_DCS - 1;
		}
		*tone = TRX_DCSCodes[*index] | 0x8000;
	}
	else
	{
		*type = CSS_CTCSS;
		*index = 0;
		*tone = TRX_CTCSSTones[*index];
	}
	return;
}

static void cssIncrementFromEvent(uiEvent_t *ev, uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	if (ev->buttons & BUTTON_SK2)
	{
		switch (*type)
		{
			case CSS_CTCSS:
				if (*index < (TRX_NUM_CTCSS - 1))
				{
					*index = (TRX_NUM_CTCSS - 1);
					*tone = TRX_CTCSSTones[*index];
				}
				else
				{
					*type = CSS_DCS;
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				break;
			case CSS_DCS:
				if (*index < (TRX_NUM_DCS - 1))
				{
					*index = (TRX_NUM_DCS - 1);
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				break;
			case CSS_NONE:
				*type = CSS_CTCSS;
				*index = 0;
				*tone = TRX_CTCSSTones[*index];
				break;
		}
	}
	else
	{
		// Step +5, cssIncrement() handles index overflow
		if (ev->keys.event & KEY_MOD_LONG)
		{
			*index += 2;
		}
		cssIncrement(tone, index, type);
	}
}

static void cssDecrement(uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	(*index)--;
	if (*type == CSS_CTCSS)
	{
		if (*index < 0)
		{
			*type = CSS_NONE;
			*index = 0;
			*tone = CODEPLUG_CSS_NONE;
			return;
		}
		*tone = TRX_CTCSSTones[*index];
	}
	else if (*type == CSS_DCS)
	{
		if (*index < 0)
		{
			*type = CSS_CTCSS;
			*index = TRX_NUM_CTCSS - 1;
			*tone = TRX_CTCSSTones[*index];
			return;
		}
		*tone = TRX_DCSCodes[*index] | 0x8000;
	}
	else
	{
		*index = 0;
		*tone = CODEPLUG_CSS_NONE;
	}
}

static void cssDecrementFromEvent(uiEvent_t *ev, uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	if (ev->buttons & BUTTON_SK2)
	{
		switch (*type)
		{
			case CSS_CTCSS:
				if (*index > 0)
				{
					*index = 0;
					*tone = TRX_CTCSSTones[*index];
				}
				else
				{
					*type = CSS_NONE;
					*index = 0;
					*tone = CODEPLUG_CSS_NONE;
				}
				break;
			case CSS_DCS:
				if (*index > 0)
				{
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				else
				{
					*type = CSS_CTCSS;
					*index = (TRX_NUM_CTCSS - 1);
					*tone = TRX_CTCSSTones[*index];
				}
				break;
			case CSS_NONE:
				break;
		}
	}
	else
	{
		// Step -5, cssDecrement() handles index < 0
		if (ev->keys.event & KEY_MOD_LONG)
		{
			*index -= 2;
		}
		cssDecrement(tone, index, type);
	}
}

int menuChannelDetails(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		freq_enter_idx = 0;
		memcpy(&tmpChannel, currentChannelData,sizeof(struct_codeplugChannel_t));

		if (codeplugChannelToneIsDCS(tmpChannel.txTone))
		{
			TxCSSType = CSS_DCS;
		}
		else if (codeplugChannelToneIsCTCSS(tmpChannel.txTone))
		{
			TxCSSType = CSS_CTCSS;
		}
		TxCSSIndex = cssIndex(tmpChannel.txTone, TxCSSType);

		if (codeplugChannelToneIsDCS(tmpChannel.rxTone))
		{
			RxCSSType = CSS_DCS;
		}
		else if (codeplugChannelToneIsCTCSS(tmpChannel.rxTone))
		{
			RxCSSType = CSS_CTCSS;
		}
		RxCSSIndex = cssIndex(tmpChannel.rxTone, RxCSSType);


		codeplugUtilConvertBufToString(tmpChannel.name, channelName, 16);
		namePos = strlen(channelName);

		if ((settingsCurrentChannelNumber == -1) && (namePos == 0)) // In VFO, and VFO has no name in the codeplug
		{
			snprintf(channelName, 17, "VFO %s", (nonVolatileSettings.currentVFONumber==0?"A":"B"));
			namePos = 5;
		}

		updateScreen();
		updateCursor(true);
	}
	else
	{
		updateCursor(false);
		if (ev->hasEvent)
			handleEvent(ev);
	}
	return 0;
}

static void updateCursor(bool moved)
{
	if (settingsCurrentChannelNumber != -1)
	{
		switch (gMenusCurrentItemIndex)
		{
		case CH_DETAILS_NAME:
			menuUpdateCursor(namePos, moved, true);
			break;
		}
	}
}

static void updateScreen(void)
{
	int mNum = 0;
	static const int bufferLen = 17;
	char buf[bufferLen];
	int tmpVal;
	int val_before_dp;
	int val_after_dp;
	struct_codeplugRxGroup_t rxGroupBuf;
	char rxNameBuf[bufferLen];

	ucClearBuf();
	menuDisplayTitle(currentLanguage->channel_details);

	if (freq_enter_idx != 0)
	{
		snprintf(buf, bufferLen, "%c%c%c.%c%c%c%c%c MHz", freq_enter_digits[0], freq_enter_digits[1], freq_enter_digits[2],
				freq_enter_digits[3], freq_enter_digits[4], freq_enter_digits[5], freq_enter_digits[6], freq_enter_digits[7]);
		ucPrintCentered(32, buf, FONT_SIZE_3);
	}
	else
	{
		keypadAlphaEnable = (gMenusCurrentItemIndex == CH_DETAILS_NAME);

		// Can only display 3 of the options at a time menu at -1, 0 and +1
		for(int i = -1; i <= 1; i++)
		{
			mNum = menuGetMenuOffset(NUM_CH_DETAILS_ITEMS, i);
			buf[0] = 0;

			switch(mNum)
			{
				case CH_DETAILS_NAME:
					strncpy(buf, channelName, 17);
				break;
				case CH_DETAILS_MODE:
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
					snprintf(buf, bufferLen, "%s:%s", currentLanguage->mode, ((tmpChannel.flag4 & 0x02) == 0x02) ? "FM" : "FMN");
					}
					else
					{
						snprintf(buf, bufferLen, "%s:DMR", currentLanguage->mode);
					}
					break;
				break;
				case CH_DETAILS_DMR_CC:
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->colour_code, currentLanguage->n_a);
					}
					else
					{
						snprintf(buf, bufferLen, "%s:%d", currentLanguage->colour_code, tmpChannel.rxColor);
					}
					break;
				case CH_DETAILS_DMR_TS:
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->timeSlot, currentLanguage->n_a);
					}
					else
					{
						snprintf(buf, bufferLen, "%s:%d", currentLanguage->timeSlot, ((tmpChannel.flag2 & 0x40) >> 6) + 1);
					}
					break;
				case CH_DETAILS_RXCTCSS:
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						if (codeplugChannelToneIsCTCSS(tmpChannel.rxTone))
						{
							snprintf(buf, bufferLen, "Rx CTCSS:%d.%dHz", tmpChannel.rxTone / 10 , tmpChannel.rxTone % 10);
						}
						else if (codeplugChannelToneIsDCS(tmpChannel.rxTone))
						{
							snprintf(buf, bufferLen, "Rx DCS:D%03oN", tmpChannel.rxTone & 0777);
						}
						else
						{
							snprintf(buf, bufferLen, "Rx CSS:%s", currentLanguage->off);
						}
					}
					else
					{
						snprintf(buf, bufferLen, "Rx CSS:%s", currentLanguage->n_a);
					}
					break;
				case CH_DETAILS_TXCTCSS:
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						if (codeplugChannelToneIsCTCSS(tmpChannel.txTone))
						{
							snprintf(buf, bufferLen, "Tx CTCSS:%d.%dHz", tmpChannel.txTone / 10 , tmpChannel.txTone % 10);
						}
						else if (codeplugChannelToneIsDCS(tmpChannel.txTone))
						{
							snprintf(buf, bufferLen, "Tx DCS:D%03oN", tmpChannel.txTone & 0777);
						}
						else
						{
							snprintf(buf, bufferLen, "Tx CSS:%s", currentLanguage->off);
						}
					}
					else
					{
						snprintf(buf, bufferLen, "Tx CSS:%s", currentLanguage->n_a);
					}
					break;
				case CH_DETAILS_RXFREQ:
					val_before_dp = tmpChannel.rxFreq / 100000;
					val_after_dp = tmpChannel.rxFreq - val_before_dp * 100000;
					snprintf(buf, bufferLen, "Rx:%d.%05dMHz", val_before_dp, val_after_dp);
					break;
				case CH_DETAILS_TXFREQ:
					val_before_dp = tmpChannel.txFreq / 100000;
					val_after_dp = tmpChannel.txFreq - val_before_dp * 100000;
					snprintf(buf, bufferLen, "Tx:%d.%05dMHz", val_before_dp, val_after_dp);
					break;
				case CH_DETAILS_BANDWIDTH:
					// Bandwidth
					if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
					{
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->bandwidth, currentLanguage->n_a);
					}
					else
					{
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->bandwidth, ((tmpChannel.flag4 & 0x02) == 0x02) ? "25kHz" : "12.5kHz");
					}
					break;
				case CH_DETAILS_FREQ_STEP:
						tmpVal = VFO_FREQ_STEP_TABLE[(tmpChannel.VFOflag5 >> 4)] / 100;
						snprintf(buf, bufferLen, "%s:%d.%02dkHz", currentLanguage->stepFreq, tmpVal, VFO_FREQ_STEP_TABLE[(tmpChannel.VFOflag5 >> 4)] - (tmpVal * 100));
					break;
				case CH_DETAILS_TOT:// TOT
					if (tmpChannel.tot != 0)
					{
						snprintf(buf, bufferLen, "%s:%ds", currentLanguage->tot, tmpChannel.tot * 15);
					}
					else
					{
						snprintf(buf, bufferLen, "%s:%s",currentLanguage->tot, currentLanguage->off);
					}
					break;
				case CH_DETAILS_ZONE_SKIP:						// Zone Scan Skip Channel (Using CPS Auto Scan flag)
					snprintf(buf, bufferLen, "%s:%s", currentLanguage->zone_skip, ((tmpChannel.flag4 & 0x20) == 0x20) ? currentLanguage->yes : currentLanguage->no);
					break;
				case CH_DETAILS_ALL_SKIP:					// All Scan Skip Channel (Using CPS Lone Worker flag)
					snprintf(buf, bufferLen, "%s:%s", currentLanguage->all_skip, ((tmpChannel.flag4 & 0x10) == 0x10) ? currentLanguage->yes : currentLanguage->no);
					break;

				case CH_DETAILS_RXGROUP:
					if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
					{
						codeplugRxGroupGetDataForIndex(tmpChannel.rxGroupList, &rxGroupBuf);
						codeplugUtilConvertBufToString(rxGroupBuf.name, rxNameBuf, 16);
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->rx_group, rxNameBuf);
					}
					else
					{
						snprintf(buf, bufferLen, "%s:%s", currentLanguage->rx_group, currentLanguage->n_a);
					}
					break;
				case CH_DETAILS_VOX:
					snprintf(buf, bufferLen, "VOX:%s",((tmpChannel.flag4 & 0x40) == 0x40) ? currentLanguage->on : currentLanguage->off);
					break;
			}

			buf[bufferLen - 1] = 0;
			menuDisplayEntry(i, mNum, buf);
		}
	}
	ucRender();
	displayLightTrigger();
}

static void updateFrequency(void)
{
	int tmp_frequency = read_freq_enter_digits(0,8);

	if (trxGetBandFromFrequency(tmp_frequency) != -1)
	{
		switch (gMenusCurrentItemIndex)
		{
			case CH_DETAILS_RXFREQ:
				tmpChannel.rxFreq = tmp_frequency;
				break;
			case CH_DETAILS_TXFREQ:
				tmpChannel.txFreq = tmp_frequency;
				break;
		}
		reset_freq_enter_digits();
	}
	else
	{
		set_melody(melody_ERROR_beep);
	}
}

static void handleEvent(uiEvent_t *ev)
{
	int tmpVal;
	struct_codeplugRxGroup_t rxGroupBuf;

	if (ev->function > 0 && ev->function < NUM_CH_DETAILS_ITEMS)
	{
		gMenusCurrentItemIndex = ev->function;
	}

	if (gMenusCurrentItemIndex == CH_DETAILS_RXFREQ || gMenusCurrentItemIndex == CH_DETAILS_TXFREQ)
	{
		if (freq_enter_idx != 0)
		{
			if (KEYCHECK_SHORTUP(ev->keys,KEY_GREEN))
			{
				updateFrequency();
				updateScreen();
				return;
			}
			if (KEYCHECK_SHORTUP(ev->keys,KEY_RED))
			{
				updateScreen();
				return;
			}
			if (KEYCHECK_SHORTUP(ev->keys,KEY_LEFT))
			{
				freq_enter_idx--;
				freq_enter_digits[freq_enter_idx] = '-';
				updateScreen();
				return;
			}
		}

		if (freq_enter_idx < 8)
		{
			int keyval = menuGetKeypadKeyValue(ev, true);
			if (keyval != 99)
			{
				freq_enter_digits[freq_enter_idx] = (char) keyval+'0';
				freq_enter_idx++;
				if (freq_enter_idx == 8)
				{
					updateFrequency();
					freq_enter_idx = 0;
				}
				updateScreen();
				return;
			}
		}
	}

	// Not entering a frequency numeric digit

	if (KEYCHECK_PRESS(ev->keys,KEY_DOWN))
	{
		MENU_INC(gMenusCurrentItemIndex, NUM_CH_DETAILS_ITEMS);
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys,KEY_UP))
	{
		MENU_DEC(gMenusCurrentItemIndex, NUM_CH_DETAILS_ITEMS);
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys,KEY_RIGHT))
	{
		switch(gMenusCurrentItemIndex)
		{
			case CH_DETAILS_NAME:
				if (settingsCurrentChannelNumber != -1)
				{
					moveCursorRightInString(channelName, &namePos, 16, (ev->buttons & BUTTON_SK2));
					updateCursor(true);
				}
				break;
			case CH_DETAILS_MODE:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.chMode = RADIO_MODE_ANALOG;
				}
				break;
			case CH_DETAILS_DMR_CC:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					if (tmpChannel.rxColor < 15)
					{
						tmpChannel.rxColor++;
						trxSetDMRColourCode(tmpChannel.rxColor);
					}
				}
				break;
			case CH_DETAILS_DMR_TS:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.flag2 |= 0x40;// set TS 2 bit
				}
				break;
			case CH_DETAILS_RXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssIncrementFromEvent(ev, &tmpChannel.rxTone, &RxCSSIndex, &RxCSSType);
					trxSetRxCSS(tmpChannel.rxTone);
				}
				break;
			case CH_DETAILS_TXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssIncrementFromEvent(ev, &tmpChannel.txTone, &TxCSSIndex, &TxCSSType);
				}
				break;
			case CH_DETAILS_BANDWIDTH:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.flag4 |= 0x02;// set 25kHz bit
				}
				break;
			case CH_DETAILS_FREQ_STEP:
				tmpVal = (tmpChannel.VFOflag5>>4) + 1;
				if (tmpVal > 7)
				{
					tmpVal = 7;
				}
				tmpChannel.VFOflag5 &= 0x0F;
				tmpChannel.VFOflag5 |= tmpVal<<4;
				break;
			case CH_DETAILS_TOT:
				if (tmpChannel.tot < 255)
				{
					tmpChannel.tot++;
				}
				break;
			case CH_DETAILS_ZONE_SKIP:
				tmpChannel.flag4 |= 0x20;// set Channel Zone Skip bit (was Auto Scan)
				break;
			case CH_DETAILS_ALL_SKIP:
				tmpChannel.flag4 |= 0x10;// set Channel All Skip bit (was Lone Worker)
				break;				
			case CH_DETAILS_RXGROUP:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpVal = tmpChannel.rxGroupList;
					tmpVal++;
					while (tmpVal < 76)
					{
						if (codeplugRxGroupGetDataForIndex(tmpVal, &rxGroupBuf))
						{
							tmpChannel.rxGroupList = tmpVal;
							break;
						}
						tmpVal++;
					}
				}
				break;
			case CH_DETAILS_VOX:
				tmpChannel.flag4 |= 0x40;
				break;
		}
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys,KEY_LEFT))
	{
		switch(gMenusCurrentItemIndex)
		{
			case CH_DETAILS_NAME:
				if (settingsCurrentChannelNumber != -1)
				{
					moveCursorLeftInString(channelName, &namePos, (ev->buttons & BUTTON_SK2));
					updateCursor(true);
				}
				break;
			case CH_DETAILS_MODE:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.chMode = RADIO_MODE_DIGITAL;
					tmpChannel.flag4 &= ~0x02;// clear 25kHz bit
				}
				break;
			case CH_DETAILS_DMR_CC:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					if (tmpChannel.rxColor > 0)
					{
						tmpChannel.rxColor--;
						trxSetDMRColourCode(tmpChannel.rxColor);
					}
				}
				break;
			case CH_DETAILS_DMR_TS:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.flag2 &= 0xBF;// Clear TS 2 bit
				}
				break;
			case CH_DETAILS_RXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssDecrementFromEvent(ev, &tmpChannel.rxTone, &RxCSSIndex, &RxCSSType);
					trxSetRxCSS(tmpChannel.rxTone);
				}
				break;
			case CH_DETAILS_TXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssDecrementFromEvent(ev, &tmpChannel.txTone, &TxCSSIndex, &TxCSSType);
				}
				break;
			case CH_DETAILS_BANDWIDTH:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.flag4 &= ~0x02;// clear 25kHz bit
				}
				break;
			case CH_DETAILS_FREQ_STEP:
				tmpVal = (tmpChannel.VFOflag5>>4) - 1;
				if (tmpVal < 0)
				{
					tmpVal = 0;
				}
				tmpChannel.VFOflag5 &= 0x0F;
				tmpChannel.VFOflag5 |= tmpVal<<4;
				break;
			case CH_DETAILS_TOT:
				if (tmpChannel.tot > 0)
				{
					tmpChannel.tot--;
				}
				break;
			case CH_DETAILS_ZONE_SKIP:
				tmpChannel.flag4 &= ~0x20;// clear Channel Zone Skip Bit (was Auto Scan bit)
				break;
			case CH_DETAILS_ALL_SKIP:
				tmpChannel.flag4 &= ~0x10;// clear Channel All Skip Bit (was Lone Worker bit)
				break;				
			case CH_DETAILS_RXGROUP:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpVal = tmpChannel.rxGroupList;
					tmpVal--;
					while (tmpVal > 0)
					{
						if (codeplugRxGroupGetDataForIndex(tmpVal, &rxGroupBuf))
						{
							tmpChannel.rxGroupList = tmpVal;
							break;
						}
						tmpVal--;
					}
				}
				break;
			case CH_DETAILS_VOX:
				tmpChannel.flag4 &= ~0x40;
				break;

		}
		updateScreen();
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_GREEN))
	{
		if (settingsCurrentChannelNumber != -1)
		{
			codeplugUtilConvertStringToBuf(channelName, (char *)&tmpChannel.name, 16);
		}
		memcpy(currentChannelData, &tmpChannel, sizeof(struct_codeplugChannel_t));

		// settingsCurrentChannelNumber is -1 when in VFO mode
		// But the VFO is stored in the nonVolatile settings, and not saved back to the codeplug
		// Also don't store this back to the codeplug unless the Function key (Blue / SK2 ) is pressed at the same time.
		if (settingsCurrentChannelNumber != -1 && (ev->buttons & BUTTON_SK2))
		{
			codeplugChannelSaveDataForIndex(settingsCurrentChannelNumber, currentChannelData);
		}
		SETTINGS_PLATFORM_SPECIFIC_SAVE_SETTINGS(true);// For Baofeng RD-5R
		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (gMenusCurrentItemIndex == CH_DETAILS_NAME && settingsCurrentChannelNumber != -1)
	{
		if (ev->keys.event == KEY_MOD_PREVIEW && namePos < 16)
		{
			channelName[namePos] = ev->keys.key;
			updateCursor(true);
			updateScreen();
		}
		if (ev->keys.event == KEY_MOD_PRESS && namePos < 16)
		{
			channelName[namePos] = ev->keys.key;
			if (namePos < strlen(channelName) && namePos < 15)
			{
				namePos++;
			}
			updateCursor(true);
			updateScreen();
		}
	}
}
