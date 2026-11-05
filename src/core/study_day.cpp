#include "study_day.h"

static bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int days_in_month(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && is_leap(y)) ? 29 : d[m - 1];
}

bool datetime_valid(const LocalDateTime& dt) {
    if (dt.year < 2000 || dt.year > 2099) return false;
    if (dt.month < 1 || dt.month > 12) return false;
    if (dt.day < 1 || dt.day > days_in_month(dt.year, dt.month)) return false;
    return dt.hour >= 0 && dt.hour < 24 && dt.minute >= 0 && dt.minute < 60 &&
           dt.second >= 0 && dt.second < 60;
}

// Howard Hinnant 的 days_from_civil / civil_from_days
static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int64_t datetime_to_epoch(const LocalDateTime& dt) {
    return days_from_civil(dt.year, dt.month, dt.day) * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

static int64_t floor_div(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

void epoch_to_datetime(int64_t epoch, LocalDateTime& dt) {
    int64_t z = floor_div(epoch, 86400);
    int64_t secs = epoch - z * 86400;
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    dt.day = (int)(doy - (153 * mp + 2) / 5 + 1);
    dt.month = (int)(mp < 10 ? mp + 3 : mp - 9);
    dt.year = (int)(yoe + era * 400 + (dt.month <= 2));
    dt.hour = (int)(secs / 3600);
    dt.minute = (int)(secs % 3600 / 60);
    dt.second = (int)(secs % 60);
}

int32_t study_day(int64_t epoch) {
    return (int32_t)floor_div(epoch - STUDY_DAY_START_HOUR * 3600, 86400);
}
