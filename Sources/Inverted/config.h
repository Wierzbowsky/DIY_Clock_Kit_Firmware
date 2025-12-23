#ifndef CONFIG_H
#define CONFIG_H

// build-time configuration options:
// CFG_SET_DATE_TIME 1 or 0
// CFG_ALARM 1 or 0
// CFG_CHIME 1 or 0
// durations are in 100 ms tackts
// defaults for the configuration options:

#ifndef CFG_ALARM_DURATION
#define CFG_ALARM_DURATION 200
#endif

#ifndef CFG_CHIME_DURATION
#define CFG_CHIME_DURATION 1
#endif


#ifndef CFG_SET_DATE_TIME
#define CFG_SET_DATE_TIME 1
#endif

#ifndef CFG_ALARM
#define CFG_ALARM 1
#endif

#ifndef CFG_CHIME
#define CFG_CHIME 1
#endif

#ifndef CFG_CHIME_HOUR_START
#define CFG_CHIME_HOUR_START 10
#endif

#ifndef CFG_CHIME_HOUR_STOP
#define CFG_CHIME_HOUR_STOP 20
#endif

#ifndef TEMP_CORRECTION
#define TEMP_CORRECTION 15
#endif

#endif // CONFIG_H

