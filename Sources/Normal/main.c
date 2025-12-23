//
// STC15F204EA DIY LED Clock
// Copyright 2016, Jens Jensen
// Modded in 2025 by Wierzbowsky [RBSC] for older versions of the DIY kits.
// Version for normal LED panels
//

#include <stc12.h>
//#include <stc15.h>
#include <stdint.h>

#include "config.h"
#include "adc.h"
#include "ds1302.h"
#include "led.h"
#include "main.h"

#define FOSC    11059200
//#define FOSC    22118400

// clear wdt
#define WDT_CLEAR()    (WDT_CONTR |= 1 << 4)

// alias for relay and buzzer outputs
//#define RELAY
#define BUZZER  P0_1

// adc channels for sensors
#define ADC_LIGHT 6 //cds
#define ADC_TEMP  7 //ntc

// button switch aliases
#define SW2     P2_6
#define S2      1
#define SW1     P2_7
#define S1      0

// button press states
#define PRESS_NONE   0
#define PRESS_SHORT  1
#define PRESS_LONG   2

// should the DP be shown, negative logic
//#define DP_OFF 0x00
//#define DP_ON  0x80

// display mode states, order is important
enum display_mode {
  #if CFG_SET_DATE_TIME == 1
  M_SET_DAY,
  M_SET_MONTH,
  M_SET_YEAR,
  M_SET_HOUR,
  M_SET_MINUTE,
  #endif

  #if CFG_ALARM == 1
  M_ALARM_HOUR,
  M_ALARM_MINUTE,
  M_ALARM_ON,
  #endif

  #if CFG_CHIME == 1
  M_CHIME_START,
  M_CHIME_STOP,
  M_CHIME_ON,
  #endif

  M_NORMAL,
  M_TEMP_DISP,
  M_DATE_DISP,
  M_YEAR_DISP,
  M_WEEKDAY_DISP,
  M_SECONDS_DISP
};

/* ------------------------------------------------------------------------- */

volatile uint8_t timerTicksNow;
// delay may be only tens of ms
void _delay_ms(uint8_t ms)
{
  uint8_t stop = timerTicksNow + ms / 10;
  while(timerTicksNow != stop);
}

// GLOBALS
uint8_t i;
uint16_t count;
int16_t temp;      // temperature sensor value
uint8_t lightval;  // light sensor value
//uint8_t lightvalnew;  // light sensor value for comparison
uint8_t beep;      // actual number of sound-request

#if CFG_ALARM == 1
#define ALARM_DURATION_NO (uint16_t)-1
uint16_t alarmDuration = ALARM_DURATION_NO;
#endif // CFG_ALARM == 1

#if CFG_CHIME == 1
#define CHIME_DURATION_NO (uint8_t)-1
uint8_t chimeDuration = CHIME_DURATION_NO;
#endif // CFG_CHIME == 1

struct ds1302_rtc rtc;
struct ram_config config;
__bit  configModified;

// to work with current time, only actualy used fields are defined
struct DateTime {
  uint8_t hour;
  uint8_t minutes;
  uint8_t seconds;
};
struct DateTime now;

void convertNow() {
  now.hour = rtc.tenhour * 10 + rtc.hour;
  now.minutes = rtc.tenminutes * 10 + rtc.minutes;
  now.seconds = rtc.tenseconds * 10 + rtc.seconds;
}

struct HourToShow {
  uint8_t tens;
  uint8_t ones;
//  uint8_t pm;
};
struct HourToShow hourToShow1, hourToShow2;

void convertHourToShow(uint8_t hour, struct HourToShow * toShow) {
  toShow->tens = hour / 10;
  toShow->ones = hour % 10;
}

volatile uint8_t displaycounter;
uint8_t dbuf[4];             // led display buffer, next state
uint8_t dbufCur[4];          // led display buffer, current state
uint8_t dmode = M_NORMAL;    // display mode state

__bit display_colon;         // flash colon

__bit  flash_d1d2;
__bit  flash_d3d4;

#define timeChanged()

volatile uint8_t debounce[2];      // switch debounce buffer
volatile uint8_t switchcount[2];

void timer0_isr() __interrupt(1) __using(1)
{
  // display refresh ISR
  // cycle thru digits one at a time
  uint8_t digit = displaycounter % 4;

  // turn off all digits, set high
  P2 |= 0x0F;

  // auto dimming, skip lighting for some cycles
  if (displaycounter % lightval < 4 ) {
    // fill digits
    P3 = dbufCur[digit];

    // Colon and dots
    if ((dmode == M_NORMAL && digit == 1 && display_colon == 1) || (dmode == M_NORMAL && digit == 3 && config.alarm_on) ||
    (dmode == M_ALARM_ON && digit == 3 && config.alarm_on) || (dmode == M_CHIME_ON && digit == 3 && config.chime_on) ||
    (dmode == M_TEMP_DISP && digit == 1) || (dmode == M_SECONDS_DISP && digit == 1))
      P3_7 = 1;

    // Keep colon on when setting time, alarm and chime
    if ((dmode == M_ALARM_HOUR || dmode == M_ALARM_MINUTE || dmode == M_ALARM_ON || dmode == M_CHIME_START || dmode == M_CHIME_STOP ||
    dmode == M_CHIME_ON || dmode == M_SET_HOUR || dmode == M_SET_MINUTE) && digit == 1)
      P3_7 = 1;

    // turn on selected digit, set low
//    if (digit == 0) P2_3 = 0;
//    else if (digit == 1) P2_2 = 0;
//    else if (digit == 2) P2_1 = 0;
//    else P2_0 = 0;
    P2 &= ~(0x08 >> digit);
  }
  displaycounter++;
}

void timer1_isr() __interrupt(3) __using(1)
{
  // debounce ISR

  uint8_t s0 = switchcount[0];
  uint8_t s1 = switchcount[1];
  uint8_t d0 = debounce[0];
  uint8_t d1 = debounce[1];

  // debouncing stuff
  // keep resetting halfway if held long
  if (s0 > 250)
    s0 = 100;
  if (s1 > 250)
    s1 = 100;

  // increment count if settled closed
  if ((d0 & 0x0F) == 0x00)
    s0++;
  else
    s0 = 0;

  if ((d1 & 0x0F) == 0x00)
    s1++;
  else
    s1 = 0;

  switchcount[0] = s0;
  switchcount[1] = s1;

  // read switch positions into sliding 8-bit window
  debounce[0] = (d0 << 1) | SW1;
  debounce[1] = (d1 << 1) | SW2;

  ++timerTicksNow;

}

void Timer0Init(void) // to match 9600  // 100ms @11.0592MHz
{
//  TL0 = 0xFF-0xB8;     // Initial timer value
  TL0 = 0xA4;          // Initial timer value
  TH0 = 0xFF;          // Initial timer value
  TF0 = 0;             // Clear TF0 flag
  TR0 = 1;             // Timer0 start run
  ET0 = 1;             // enable timer0 interrupt
  EA = 1;              // global interrupt enable
}

void Timer1Init(void) // 10ms @ 11.0592MHz
{
  TL1 = 0xD5;          // Initial timer value
  TH1 = 0xDB;          // Initial timer value
  TF1 = 0;             // Clear TF1 flag
  TR1 = 1;             // Timer1 start run
  ET1 = 1;             // enable Timer1 interrupt
  EA = 1;              // global interrupt enable
}

uint8_t getkeypress(uint8_t keynum)
{
  if (switchcount[keynum] > 150) {
    _delay_ms(30);
    return PRESS_LONG;  // ~1.5 sec
  }
  if (switchcount[keynum]) {
    _delay_ms(60);
    return PRESS_SHORT; // ~100 msec
  }
  return PRESS_NONE;
}

// store display bytes
#define displayChar(pos, val) dbuf[pos] = ledtable[val]

//#define displayDp(pos) dbuf[pos] &= ~DP_ON

void display(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4)
{
  if(flash_d1d2) {
    displayChar(0, LED_BLANK);
    displayChar(1, LED_BLANK);
  }
  else {
    displayChar(0, d1);
    displayChar(1, d2);
  }

  if(flash_d3d4) {
    displayChar(2, LED_BLANK);
    displayChar(3, LED_BLANK);
  }
  else {
    displayChar(2, d3);
    displayChar(3, d4);
  }

//  if(display_colon) {
//    displayDp(1);
//  }
}

/*********************************************/
int main()
{
  // SETUP
  // set photoresistor, & ntc pins to open-drain output, already have strong pullups
  P1M1 |= (1 << 7) | (1 << 6);
  P1M0 |= (1 << 7) | (1 << 6);

  // Set Port 3 to Push-Pull to increase brightness
  P3M1 = 0x00;
  P3M0 = 0xFF; 

  // Configure P2.0, P2.1, P2.2, and P2.3 to Push-Pull mode to increase brightness
  P2M1 &= ~0x0F;
  P2M0 |= 0x0F;

  // init rtc
  ds_init();
  // init/read ram config
  ds_ram_config_init((uint8_t *) &config);

  Timer0Init(); // display refresh
  Timer1Init(); // switch debounce

  // LOOP
  while(1)
  {
    _delay_ms(100);
    count++;
    WDT_CLEAR();

    // run every ~1 secs
    if (count % 40 == 0) {
      lightval = getADCResult(ADC_LIGHT) >> 4; //dark 127 -- bright 0
//      if (lightvalnew > (lightval + 2) || lightvalnew < (lightval - 2))
//        lightval = lightvalnew;
      temp = 76 - getADCResult(ADC_TEMP) * 64 /1026 + config.temp_offset - TEMP_CORRECTION; //formula for ntc adc value to approx C

      // constrain dimming range
      if (lightval < 4)
        lightval = 4;
    }

    ds_readburst((uint8_t *) &rtc); // read rtc
    convertNow();

    #if CFG_ALARM == 1
    // check alarm
    if(alarmDuration == ALARM_DURATION_NO) {
      if(config.alarm_on) {
        if(config.alarm_hour == now.hour && config.alarm_minute == now.minutes) {
          alarmDuration = CFG_ALARM_DURATION;
          ++beep;
        }
      }
    }
    else if(alarmDuration == 0) {
      if(config.alarm_hour != now.hour) {
        alarmDuration = ALARM_DURATION_NO; // forget about last alarm after one hour
      }
    }
    else {
      if(getkeypress(S1) || getkeypress(S2)) {
        alarmDuration = 0;
        --beep;
        continue; // don't interpret same key again
      }
      --alarmDuration;
      if(alarmDuration == 0) --beep;
    }
    #endif // CFG_ALARM == 1

    #if CFG_CHIME == 1
    // check chime
    if(chimeDuration == CHIME_DURATION_NO) {
      if(config.chime_on) {
        if(now.minutes == 0 && now.seconds == 0) {
          if((config.chime_hour_start <= config.chime_hour_stop && config.chime_hour_start <= now.hour && now.hour <= config.chime_hour_stop)
            || (config.chime_hour_start > config.chime_hour_stop && (config.chime_hour_start <= now.hour || now.hour <= config.chime_hour_stop)))
          {
            chimeDuration = CFG_CHIME_DURATION;
            ++beep;
          }
        }
      }
    }
    else if(chimeDuration == 0) {
      if(now.minutes != 0) {
        chimeDuration = CHIME_DURATION_NO; // forget about last chime
      }
    }
    else {
      --chimeDuration;
      if(chimeDuration == 0) --beep;
    }
    #endif // CFG_CHIME == 1

    BUZZER = (beep ? 0 : 1);

    // display decision tree
    display_colon = 0;
    switch (dmode)
    {
      #if CFG_SET_DATE_TIME == 1
      case M_SET_HOUR:
        display_colon = 1;
        flash_d1d2 = !flash_d1d2;
        if (getkeypress(S2)) {
          ds_hours_incr(now.hour);
          timeChanged();
        }
        if (getkeypress(S1)) {
          flash_d1d2 = 0;
          dmode = M_SET_MINUTE;
        }
        break;

      case M_SET_MINUTE:
        display_colon = 1;
        flash_d3d4 = !flash_d3d4;
        if (getkeypress(S2)) {
          ds_minutes_incr(now.minutes);
          timeChanged();
        }
        if (getkeypress(S1)) {
          flash_d3d4 = 0;
          ++dmode; // M_ALARM_HOUR, M_CHIME_START or M_NORMAL
        }
        break;

      case M_SET_DAY:
        flash_d1d2 = !flash_d1d2;
        if (getkeypress(S2)) {
          ds_day_incr(&rtc);
          timeChanged();
        }
        if (getkeypress(S1)) {
          flash_d1d2 = 0;
          dmode = M_SET_MONTH;
        }
        break;

      case M_SET_MONTH:
        flash_d3d4 = !flash_d3d4;
        if (getkeypress(S2)) {
          ds_month_incr(&rtc);
          timeChanged();
        }
        if (getkeypress(S1)) {
          flash_d3d4 = 0;
          dmode = M_SET_YEAR;
        }
        break;

      case M_SET_YEAR:
        flash_d3d4 = !flash_d3d4;
        if (getkeypress(S2)) {
          ds_year_incr(&rtc);
          timeChanged();
        }
        if (getkeypress(S1)) {
          flash_d3d4 = 0;
          dmode = M_WEEKDAY_DISP;
        }
        break;

      #endif // CFG_SET_DATE_TIME == 1

      #if CFG_ALARM == 1
      case M_ALARM_HOUR:
        display_colon = 1;
        flash_d1d2 = !flash_d1d2;
        if(getkeypress(S2)) {
          ++config.alarm_hour;
          if(config.alarm_hour >= 24) config.alarm_hour = 0;
          config.alarm_on = 1;
          alarmDuration = ALARM_DURATION_NO; // reset alarm state
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d1d2 = 0;
          dmode = M_ALARM_MINUTE;
        }
        break;

      case M_ALARM_MINUTE:
        display_colon = 1;
        flash_d3d4 = !flash_d3d4;
        if(getkeypress(S2)) {
          ++config.alarm_minute;
          if(config.alarm_minute >= 60) config.alarm_minute = 0;
          config.alarm_on = 1;
          alarmDuration = ALARM_DURATION_NO; // reset alarm state
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d3d4 = 0;
          dmode = M_ALARM_ON;
        }
        break;

      case M_ALARM_ON:
        display_colon = 1;
        flash_d1d2 = !flash_d1d2;
        flash_d3d4 = !flash_d3d4;
        if(getkeypress(S2)) {
          config.alarm_on = !config.alarm_on;
          alarmDuration = ALARM_DURATION_NO; // reset alarm state
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d1d2 = 0;
          flash_d3d4 = 0;
          ++dmode; // M_CHIME_START or M_NORMAL
        }
        break;
      #endif // CFG_ALARM == 1

      #if CFG_CHIME == 1
      case M_CHIME_START:
        display_colon = 1;
        flash_d1d2 = !flash_d1d2;
        if(getkeypress(S2)) {
          ++config.chime_hour_start;
          if(config.chime_hour_start >= 24) config.chime_hour_start = 0;
          config.chime_on = 1;
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d1d2 = 0;
          dmode = M_CHIME_STOP;
        }
        break;

      case M_CHIME_STOP:
        display_colon = 1;
        flash_d3d4 = !flash_d3d4;
        if(getkeypress(S2)) {
          ++config.chime_hour_stop;
          if(config.chime_hour_stop >= 24) config.chime_hour_stop = 0;
          config.chime_on = 1;
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d3d4 = 0;
          dmode = M_CHIME_ON;
        }
        break;

      case M_CHIME_ON:
        display_colon = 1;
        flash_d1d2 = !flash_d1d2;
        flash_d3d4 = !flash_d3d4;
        if(getkeypress(S2)) {
          config.chime_on = !config.chime_on;
          configModified = 1;
        }
        if(getkeypress(S1)) {
          flash_d1d2 = 0;
          flash_d3d4 = 0;
          ++dmode; // M_NORMAL
        }
        break;
      #endif // CFG_CHIME == 1

      case M_TEMP_DISP:
        display_colon = 1;
        if (getkeypress(S1)) {
          config.temp_offset++;
          if(config.temp_offset > 10) config.temp_offset = -10;
          configModified = 1;
        }
        if (getkeypress(S2))
          dmode = M_DATE_DISP;
        break;

      case M_DATE_DISP:
        #if CFG_SET_DATE_TIME == 1
        if (getkeypress(S1))
          dmode = M_SET_DAY;
        #endif // CFG_SET_DATE_TIME == 1

        if (getkeypress(S2))
          dmode = M_YEAR_DISP;
        break;

      case M_YEAR_DISP:
        #if CFG_SET_DATE_TIME == 1
        if (getkeypress(S1))
          dmode = M_SET_YEAR;
        #endif // CFG_SET_DATE_TIME == 1

        if (getkeypress(S2))
          dmode = M_WEEKDAY_DISP;
        break;

      case M_WEEKDAY_DISP:
        #if CFG_SET_DATE_TIME == 1
        if (getkeypress(S1)) {
          ds_weekday_incr(&rtc);
          timeChanged();
        }
        #endif // CFG_SET_DATE_TIME == 1

        if (getkeypress(S2))
          dmode = M_SECONDS_DISP;
        break;

      case M_SECONDS_DISP:
        if (count % 10 < 5)
          display_colon = 1;

        #if CFG_SET_DATE_TIME == 1
        if (getkeypress(S1)) {
          ds_seconds_reset();
          timeChanged();
        }
        #endif // CFG_SET_DATE_TIME == 1

        if (getkeypress(S2))
          dmode = M_NORMAL;
        break;

      case M_NORMAL:
      default:
        if (count % 10 < 5)
          display_colon = 1;

        #if CFG_SET_DATE_TIME == 1
        if (getkeypress(S1) == PRESS_LONG && getkeypress(S2) == PRESS_LONG) {
          ds_reset_clock();
          timeChanged();
        }

        if (getkeypress(S1) == PRESS_SHORT) {
          dmode = M_SET_HOUR;
        }
        #endif // CFG_SET_DATE_TIME == 1

        if (getkeypress(S2) == PRESS_SHORT) {
          dmode = M_TEMP_DISP;
        }

    };

    // display execution tree
    switch (dmode) {
      case M_NORMAL:

      // Switch display modes
      if (rtc.tenseconds == 2 && rtc.seconds >= 0 && rtc.seconds < 5)
      {
        display(ds_int2bcd_tens(temp), ds_int2bcd_ones(temp), LED_TEMP, (temp >= 0) ? LED_BLANK : LED_DASH);
        display_colon = 1;
        break;
      }
      else if (rtc.tenseconds == 2 && rtc.seconds >= 5)
      {
        display(rtc.tenday, rtc.day, rtc.tenmonth, rtc.month);
        display_colon = 0;
        break;
      }
      else if (rtc.tenseconds == 3 && rtc.seconds >= 0  && rtc.seconds < 5)
      {
        display(2, 0, rtc.tenyear, rtc.year);
        display_colon = 0;
        break;
      }
      else if (rtc.tenseconds == 3 && rtc.seconds >= 5)
      {
        display(LED_BLANK, LED_DASH, rtc.weekday, LED_DASH);
        display_colon = 0;
        break;
      }
//      else if (rtc.tenseconds == 4 && rtc.seconds >= 0 && rtc.seconds < 5)
//      {
//        display(LED_BLANK, LED_BLANK, rtc.tenseconds, rtc.seconds);
//        break;
//      }
      else
      {
      #if CFG_SET_DATE_TIME == 1
      case M_SET_HOUR:
      case M_SET_MINUTE:
      #endif

        convertHourToShow(now.hour, &hourToShow1);
	if (hourToShow1.tens == 0) display(LED_BLANK, hourToShow1.ones, rtc.tenminutes, rtc.minutes);
        else display(hourToShow1.tens, hourToShow1.ones, rtc.tenminutes, rtc.minutes);

//        #if CFG_ALARM == 1
//        if(dmode == M_NORMAL && config.alarm_on) displayDp(3);
//        #endif // CFG_ALARM == 1

        break;
      }

      case M_DATE_DISP:
      #if CFG_SET_DATE_TIME == 1
      case M_SET_MONTH:
      case M_SET_DAY:
      #endif
        display(rtc.tenday, rtc.day, rtc.tenmonth, rtc.month);
        //displayDp(1);
        break;

      case M_YEAR_DISP:
      #if CFG_SET_DATE_TIME == 1
      case M_SET_YEAR:
      #endif
        display(2, 0, rtc.tenyear, rtc.year);
        //displayDp(3);
        break;

      #if CFG_ALARM == 1
      case M_ALARM_HOUR:
      case M_ALARM_MINUTE:
      case M_ALARM_ON:
        convertHourToShow(config.alarm_hour, &hourToShow1);
        display(hourToShow1.tens, hourToShow1.ones, config.alarm_minute / 10, config.alarm_minute % 10);
//        if(config.alarm_on) displayDp(3);
        break;
      #endif

      #if CFG_CHIME == 1
      case M_CHIME_START:
      case M_CHIME_STOP:
      case M_CHIME_ON:
        convertHourToShow(config.chime_hour_start, &hourToShow1);
        convertHourToShow(config.chime_hour_stop, &hourToShow2);
        display(hourToShow1.tens, hourToShow1.ones, hourToShow2.tens, hourToShow2.ones);
//        if(config.chime_on) displayDp(3);
        break;
      #endif

      case M_WEEKDAY_DISP:
        display(LED_BLANK, LED_DASH, rtc.weekday, LED_DASH);
        break;

      case M_TEMP_DISP:
        display(ds_int2bcd_tens(temp), ds_int2bcd_ones(temp), LED_TEMP, (temp >= 0) ? LED_BLANK : LED_DASH);
//        displayDp(2);
        break;

      case M_SECONDS_DISP:
        display(LED_BLANK, LED_BLANK, rtc.tenseconds, rtc.seconds);
        break;

    }

    // Refresh the values
    dbufCur[0] = dbuf[0];
    dbufCur[1] = dbuf[1];
    dbufCur[2] = dbuf[2];
    dbufCur[3] = dbuf[3];

    // save ram config
    if(configModified) {
      ds_ram_config_write((uint8_t *) &config);
      configModified = 0;
    }
  }
}

