#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <string.h>
#include <stdio.h>

/* Minimal NMEA parser for the GPS module (e.g. NEO-6M/M8N at 9600 baud).
 * Parses GGA and RMC sentences. No external TinyGPS dependency.
 */
class GPS {
public:
    GPS();
    void begin(HardwareSerial &port, uint32_t baud, uint8_t rx_gpio,
               uint8_t tx_gpio);

    /* Feed bytes. Call regularly from loop. Returns true if a new fix
     * was updated since last position() call. */
    void update();

    bool  valid()   const { return _valid; }
    double latitude()  const { return _latitude; }
    double longitude() const { return _longitude; }
    uint8_t satellites() const { return _satellites; }
    uint32_t fix_age_ms() const { return (uint32_t)(millis() - _fix_time_ms); }
    bool has_fix() const { return _valid && (fix_age_ms() < 5000); }

    const char *last_error() const { return _last_error; }

private:
    HardwareSerial *_port;
    bool _valid;
    double _latitude, _longitude;
    uint8_t _satellites;
    uint32_t _fix_time_ms;
    char _last_error[64];
    char _line[120];
    size_t _line_len;

    bool parse_gga(const char *line);
    bool parse_rmc(const char *line);
    static double conv_to_deg(const char *val, char hem);
};

#endif /* GPS_H */
