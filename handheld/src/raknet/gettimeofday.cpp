



#if defined(_WIN32) && !defined(__GNUC__)  &&!defined(__GCCXML__)

#include "gettimeofday.h"

// From http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/

#include "WindowsIncludes.h"
#include <time.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
#if !defined(MCPE_UWP)
  static int tzflag;
#endif

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    /*converting file time to unix epoch*/
    tmpres /= 10;  /*convert into microseconds*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
#if defined(MCPE_UWP)
    // The UWP CRT intentionally omits the desktop-only _tzset entry point.
    // Use its supported accessors so the RakNet timeval compatibility layer
    // still reports the local UTC offset and daylight-saving state.
    long timezoneSeconds = 0;
    int daylight = 0;
    _get_timezone(&timezoneSeconds);
    _get_daylight(&daylight);
    tz->tz_minuteswest = static_cast<int>(timezoneSeconds / 60);
    tz->tz_dsttime = daylight;
#else
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
#endif
  }

  return 0;
}

#endif

