#include "rtc.h"
#include "../cpu/ports.h"
#include "../libc/string.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

//setting rtc flag
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
    while (get_update_in_progress_flag());
    
    uint8_t second = get_RTC_register(0x00);
    uint8_t minute = get_RTC_register(0x02);
    uint8_t hour   = get_RTC_register(0x04);
    uint8_t week   = get_RTC_register(0x06);
    uint8_t day    = get_RTC_register(0x07);
    uint8_t month  = get_RTC_register(0x08);
    uint8_t year   = get_RTC_register(0x09);

    
    uint8_t registerB = get_RTC_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    // Format: YYYY-MM-DD HH:MM:SS
    // Simple implementation of itoa for the fixed widths
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

    //Week
    switch (week){
        case 1:
            strcat(time_str, "Sun");
            break;
        case 2:
            strcat(time_str, "Mon");
            break;
        case 3:
            strcat(time_str, "Tue");
            break;
        case 4:
            strcat(time_str, "Wed");
            break;
        case 5:
            strcat(time_str, "Thu");
            break;
        case 6:
            strcat(time_str, "Fri");
            break;
        case 7:
            strcat(time_str, "Sat");
            break;
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
