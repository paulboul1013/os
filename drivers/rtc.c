#include "rtc.h"
#include "../cpu/ports.h"
#include "../libc/string.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

//check UIP bit status 
int get_update_in_progress_flag() {
    port_byte_out(CMOS_ADDRESS, 0x0A);
    return (port_byte_in(CMOS_DATA) & 0x80);
}

//get rtc register value
// like 0x00 register is get the second value
uint8_t get_RTC_register(int reg) {
    port_byte_out(CMOS_ADDRESS, reg);
    return port_byte_in(CMOS_DATA);
}

void get_time(char* time_str) {
    uint8_t second, minute, hour, day, month, year, week;
    uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year, last_week;
    uint8_t registerB;

    // Wait for update finish (UIP == 0)
    while (get_update_in_progress_flag());
    
    second = get_RTC_register(0x00);
    minute = get_RTC_register(0x02);
    hour   = get_RTC_register(0x04);
    week   = get_RTC_register(0x06);
    day    = get_RTC_register(0x07);
    month  = get_RTC_register(0x08);
    year   = get_RTC_register(0x09);

    /**
     * 安全性增強：二次讀取比對法 (Read-Until-Consistent)
     * 即使檢查了 UIP 位元，仍可能在讀取暫存期間發生進位。
     * 透過連續兩次讀取並比對，確保取得的時間資料是完全一致的。
     */
    do {
        last_second = second;
        last_minute = minute;
        last_hour   = hour;
        last_week   = week;
        last_day    = day;
        last_month  = month;
        last_year   = year;

        while (get_update_in_progress_flag());
        second = get_RTC_register(0x00);
        minute = get_RTC_register(0x02);
        hour   = get_RTC_register(0x04);
        week   = get_RTC_register(0x06);
        day    = get_RTC_register(0x07);
        month  = get_RTC_register(0x08);
        year   = get_RTC_register(0x09);
    } while ( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
              (last_week != week) || (last_day != day) || (last_month != month) || (last_year != year) );

    registerB = get_RTC_register(0x0B);

    // Convert BCD to binary check if register B is in BCD mode
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
    }

    // Convert 12 hour clock to 24 hour clock check if register B is in 12 hour mode
    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    // Format: YYYY-MM-DD HH:MM:SS
    char buf[5];
    
    // Year (assuming 20xx)
    int_to_ascii(2000 + year, buf);
    strcat(time_str, buf);
    strcat(time_str, "-");
    
    // Month
    if (month < 10) strcat(time_str, "0");
    int_to_ascii(month, buf);
    strcat(time_str, buf);
    strcat(time_str, "-");
    
    // Day
    if (day < 10) strcat(time_str, "0");
    int_to_ascii(day, buf);
    strcat(time_str, buf);
    strcat(time_str, " ");

    // Week handling
    switch (week){
        case 1: strcat(time_str, "Sun"); break;
        case 2: strcat(time_str, "Mon"); break;
        case 3: strcat(time_str, "Tue"); break;
        case 4: strcat(time_str, "Wed"); break;
        case 5: strcat(time_str, "Thu"); break;
        case 6: strcat(time_str, "Fri"); break;
        case 7: strcat(time_str, "Sat"); break;
    }
    strcat(time_str, " ");
    
    // Hour
    if (hour < 10) strcat(time_str, "0");
    int_to_ascii(hour, buf);
    strcat(time_str, buf);
    strcat(time_str, ":");
    
    // Minutes
    if (minute < 10) strcat(time_str, "0");
    int_to_ascii(minute, buf);
    strcat(time_str, buf);
    strcat(time_str, ":");
    
    // Seconds
    if (second < 10) strcat(time_str, "0");
    int_to_ascii(second, buf);
    strcat(time_str, buf);
}
