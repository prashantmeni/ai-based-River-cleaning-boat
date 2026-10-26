#include "gps.h"

GPS::GPS() : _port(nullptr), _valid(false), _latitude(0), _longitude(0),
             _satellites(0), _fix_time_ms(0), _line_len(0) {
    _last_error[0] = '\0';
}

void GPS::begin(HardwareSerial &port, uint32_t baud, uint8_t rx_gpio,
                uint8_t tx_gpio) {
    _port = &port;
    _port->begin(baud, SERIAL_8N1, rx_gpio, tx_gpio);
}

double GPS::conv_to_deg(const char *val, char hem) {
    if (!val || !*val) return 0.0;
    double raw = atof(val);
    int deg = (int)(raw / 100.0);
    double minutes = raw - deg * 100.0;
    double d = deg + minutes / 60.0;
    if (hem == 'S' || hem == 'W') d = -d;
    return d;
}

void GPS::update() {
    while (_port && _port->available()) {
        char c = (char)_port->read();
        if (c == '\n' || c == '\r') {
            if (_line_len > 0) {
                _line[_line_len] = '\0';
                if (_line_len > 6 && strncmp(_line, "$GP", 3) == 0) {
                    if (strstr(_line, "GGA,")) parse_gga(_line);
                    else if (strstr(_line, "RMC,")) parse_rmc(_line);
                }
                _line_len = 0;
            }
        } else if (_line_len < sizeof(_line) - 1) {
            _line[_line_len++] = c;
        }
    }
}

bool GPS::parse_gga(const char *line) {
    /* $GPGGA,hhmmss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh */
    char *fields[16];
    int nf = 0;
    char buf[120];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, ",");
    while (tok && nf < 16) { fields[nf++] = tok; tok = strtok(nullptr, ","); }
    if (nf < 7) return false;
    if (fields[6][0] == '0') { _valid = false; return false; }
    double lat = conv_to_deg(fields[2], fields[3] ? fields[3][0] : ' ');
    double lon = conv_to_deg(fields[4], fields[5] ? fields[5][0] : ' ');
    _latitude = lat; _longitude = lon;
    _satellites = (uint8_t)atoi(fields[7]);
    _valid = true;
    _fix_time_ms = millis();
    return true;
}

bool GPS::parse_rmc(const char *line) {
    /* $GPRMC,hhmmss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,... */
    char *fields[14];
    int nf = 0;
    char buf[120];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, ",");
    while (tok && nf < 14) { fields[nf++] = tok; tok = strtok(nullptr, ","); }
    if (nf < 7) return false;
    bool active = (fields[2][0] == 'A');
    if (!active) { _valid = false; return false; }
    double lat = conv_to_deg(fields[3], fields[4] ? fields[4][0] : ' ');
    double lon = conv_to_deg(fields[5], fields[6] ? fields[6][0] : ' ');
    _latitude = lat; _longitude = lon;
    _valid = true;
    _fix_time_ms = millis();
    return true;
}
