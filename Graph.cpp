// Graph class for m5stack CO2 Sensor
//
// October 28, 2025
// Tetsu Nishimura

#include <float.h>
#include "Graph.h"

Graph::Graph() : _sprite(&M5.Display) {}

/**
 * @brief Initialize graph with given width, height, x minimum, y minimum, y grid
 * and colors for each values.
 *
 * @param width Width of the graph
 * @param height Height of the graph
 * @param xmin X minimum of the graph
 * @param ymin Y minimum of the graph
 * @param ygrid Y grid of the graph
 * @param crVal1 Color for the first value
 * @param crVal2 Color for the second value
 * @param crVal3 Color for the third value
 */
void Graph::begin(uint16_t width, uint16_t height, uint16_t xmin, uint16_t ymin, uint16_t ygrid, uint32_t crVal1, uint32_t crVal2, uint32_t crVal3) {
    memset(_val1, 0, sizeof(_val1));
    memset(_val2, 0, sizeof(_val2));
    memset(_val3, 0, sizeof(_val3));
    _wp = _rp = 0;
    _width = width;
    _height = height;
    _xmin = xmin;
    _screen_ymin = ymin;
    _ymin = 0;
    _ymid = _ymin + height / 2;
    _ymax = _ymin + height;
    _ygrid = ygrid;
    _crVal1 = crVal1;
    _crVal2 = crVal2;
    _crVal3 = crVal3;

    _sprite.setColorDepth(16);
    _sprite.createSprite(320, _height + 2);
}

/**
 * @brief Add new data points to the graph
 *
 * @param val1 First value to be added
 * @param val2 Second value to be added
 * @param val3 Third value to be added
 */
void Graph::add(float val1, float val2, float val3) {
    _val1[_wp] = val1;
    _val2[_wp] = val2;
    _val3[_wp] = val3;
    _wp++;
    _wp &= BUF_MASK;

    if (((_wp - _rp) & BUF_MASK) > _width) {
        _rp++;
        _rp &= BUF_MASK;
    }
}

void Graph::plot(m5::rtc_datetime_t RTCdata, uint8_t labels) {
    int8_t hour = RTCdata.time.hours;
    uint8_t minute = RTCdata.time.minutes;

    // Clear graph
    _sprite.fillSprite(TFT_BLACK);
    _sprite.setTextFont(0);
    _sprite.setTextDatum(0);
    _sprite.setTextColor(TFT_WHITE);

    // draw vertical grid
    for (int n = _xmin + _width - 1 - minute; n > _xmin; n -= 60) {
        _sprite.drawLine(n, _ymin, n, _ymax, TFT_LIGHTGREY);

        if (n + 20 < _xmin + _width) {
            _sprite.drawNumber(hour, n + 2, _ymax - 8);
        }

        if (--hour < 0) {
            hour = 23;
        }
    }

    // draw horizontal grid
    _sprite.drawLine(_xmin, _ymid, _xmin + _width - 1, _ymid, TFT_DARKGREY);
    for (int n = _ygrid; n < _height / 2; n += _ygrid) {
        _sprite.drawLine(_xmin, _ymid - n, _xmin + _width - 1, _ymid - n, TFT_DARKGREY);
        _sprite.drawLine(_xmin, _ymid + n, _xmin + _width - 1, _ymid + n, TFT_DARKGREY);
    }

    // plot line
    drawline(_val1, 200, _crVal1, labels & 0x01, _xmin);
    drawline(_val2, 2, _crVal2, labels & 0x02, 306);
    drawline(_val3, 5, _crVal3, labels & 0x04, 319);

    _sprite.pushSprite(0, _screen_ymin);
}

void Graph::drawline(float* values, uint16_t ystep, uint32_t color, boolean label, uint16_t labelPos) {
    float offset = getOffset(values);
    uint16_t y;

    // check if the latest point is out of range
    // _ymin < _ymid - _ygrid * (values[_wp - 1] - offset) / ystep < _ymax
    // values[_wp - 1] - span / 2 < offset < values[_wp - 1] + span / 2
    float dataSpan = _height * ystep / _ygrid;
    float offsetLow = values[_wp - 1] - dataSpan / 2;
    float offsetHigh = values[_wp - 1] + dataSpan / 2;
    if (offset < offsetLow) {
        // offset = offsetLow;
        offset = ceilf(offsetLow / ystep) * ystep;
    }
    else if (offset > offsetHigh) {
        // offset = offsetHigh;
        offset = floorf(offsetHigh / ystep) * ystep;
    }
    else {
        offset = roundf(offset / ystep) * ystep; // round
    }

    // draw line
    for (int i = 0, pt = _wp - 1; i < ((_wp - _rp) & BUF_MASK); i++) {
        y = _ymid - _ygrid * (values[pt] - offset) / ystep;
        pt--;
        pt &= BUF_MASK;

        if (y > _ymin && y < _ymax) {
            // _sprite.drawPixel(_xmin + _width - 1 - i, y - 1, color);
            // _sprite.drawPixel(_xmin + _width - 1 - i, y    , color);
            // _sprite.drawPixel(_xmin + _width - 1 - i, y + 1, color);
            _sprite.drawLine(_xmin + _width - 1 - i, y - 1, _xmin + _width - 1 - i, y + 1, color);
        }

        if (label) {
            _sprite.setTextDatum(2);
            _sprite.setTextColor(color);
            _sprite.drawNumber(offset, labelPos, _ymid - 4);
            for (int n = 1; n < _height / _ygrid / 2; n++) {
                _sprite.drawNumber(offset + n * ystep, labelPos, _ymid - 4 - n * _ygrid);
                _sprite.drawNumber(offset - n * ystep, labelPos, _ymid - 4 + n * _ygrid);
            }
        }
    }
}

float Graph::getOffset(float* value) {
    int n = _rp;
    float vmin = FLT_MAX;
    float vmax = FLT_MIN;

    while (n != _wp) {
        vmin = min(vmin, value[n]);
        vmax = max(vmax, value[n]);
        n++;
        n &= BUF_MASK;
    }

    return (vmin + vmax) / 2;
}

