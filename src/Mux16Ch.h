#pragma once
#include <Arduino.h>

template<uint8_t MAX_DEVICES = 8>
class BusMUX16
{
public:
    enum DevType : uint8_t { DEV_ANALOG = 0, DEV_DIGITAL = 1 };

    BusMUX16(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3)
        : _s0(s0), _s1(s1), _s2(s2), _s3(s3) {}

    uint8_t addAnalogDevice(uint8_t sigPin, bool invert=false, uint8_t smoothing=8)
    {
        if (_devCount >= MAX_DEVICES) return 255;
        Device &d = _dev[_devCount];
        d.type = DEV_ANALOG;
        d.sigPin = sigPin;
        d.invert = invert;
        d.pullup = false;
        d.smoothing = smoothing ? smoothing : 1;
        d.debounce = 0;
        initAnalogBuffers(d);
        return _devCount++;
    }

    uint8_t addDigitalDevice(uint8_t sigPin, bool pullup=true, bool invert=false, uint8_t debounceSamples=3)
    {
        if (_devCount >= MAX_DEVICES) return 255;
        Device &d = _dev[_devCount];
        d.type = DEV_DIGITAL;
        d.sigPin = sigPin;
        d.invert = invert;
        d.pullup = pullup;
        d.smoothing = 1;
        d.debounce = debounceSamples ? debounceSamples : 1;
        initDigitalBuffers(d);
        return _devCount++;
    }

    void setMinSettleMicros(uint16_t us) { _minSettleUs = us; }

    void setup()
    {
        pinMode(_s0, OUTPUT); pinMode(_s1, OUTPUT);
        pinMode(_s2, OUTPUT); pinMode(_s3, OUTPUT);

        for (uint8_t i = 0; i < _devCount; ++i) {
            if (_dev[i].type == DEV_ANALOG) {
                pinMode(_dev[i].sigPin, INPUT);
            } else {
                pinMode(_dev[i].sigPin, _dev[i].pullup ? INPUT_PULLUP : INPUT);
            }
        }

        _currentCh = 0;
        _writeAddr(0); // starts settle timer
    }

    void loop()
    {
        if (micros() - _lastStepUs < _minSettleUs) return;

        // Sample all devices at this channel
        for (uint8_t i = 0; i < _devCount; ++i)
        {
            Device &d = _dev[i];
            if (d.type == DEV_ANALOG)
            {
                uint16_t raw = (uint16_t)analogRead(d.sigPin);
                d.a.raw[_currentCh] = raw;

                if (!d.a.seeded[_currentCh]) {
                    d.a.filt[_currentCh] = raw;
                    d.a.seeded[_currentCh] = true;
                } else {
                    int32_t diff = (int32_t)raw - (int32_t)d.a.filt[_currentCh];
                    d.a.filt[_currentCh] = (uint16_t)((int32_t)d.a.filt[_currentCh] + diff / (int32_t)d.smoothing);
                }
            }
            else // DEV_DIGITAL
            {
                uint8_t sample = (uint8_t)digitalRead(d.sigPin);
                if (d.invert) sample = sample ? LOW : HIGH;

                if (sample == d.d.lastSample[_currentCh]) {
                    if (d.d.count[_currentCh] < 250) d.d.count[_currentCh]++;
                } else {
                    d.d.count[_currentCh] = 1;
                    d.d.lastSample[_currentCh] = sample;
                }
                if (d.d.count[_currentCh] >= d.debounce) {
                    d.d.stable[_currentCh] = sample;
                }
            }
        }

        // Next channel
        _currentCh = (uint8_t)((_currentCh + 1) & 0x0F);
        _writeAddr(_currentCh); // resets settle timer
    }

    // Introspection
    uint8_t deviceCount() const { return _devCount; }
    DevType deviceType(uint8_t i) const { return (i < _devCount) ? _dev[i].type : DEV_ANALOG; }
    uint8_t devicePin(uint8_t i) const { return (i < _devCount) ? _dev[i].sigPin : 0; }
    uint8_t currentChannel() const { return _currentCh; }

    // Getters
    uint16_t getAnalogRaw(uint8_t dev, uint8_t ch) const
    {
        if (dev >= _devCount || ch > 15 || _dev[dev].type != DEV_ANALOG) return 0;
        return _dev[dev].a.raw[ch];
    }
    uint16_t getAnalogFiltered(uint8_t dev, uint8_t ch) const
    {
        if (dev >= _devCount || ch > 15 || _dev[dev].type != DEV_ANALOG) return 0;
        return _dev[dev].a.filt[ch];
    }
    uint8_t getAnalog8(uint8_t dev, uint8_t ch, int minRaw=0, int maxRaw=1023) const
    {
        if (dev >= _devCount || ch > 15 || _dev[dev].type != DEV_ANALOG) return 0;
        uint16_t v = _dev[dev].a.filt[ch];
        if (minRaw > maxRaw) { int t = minRaw; minRaw = maxRaw; maxRaw = t; }
        if (v < (uint16_t)minRaw) v = (uint16_t)minRaw;
        if (v > (uint16_t)maxRaw) v = (uint16_t)maxRaw;
        uint16_t span = (uint16_t)(maxRaw - minRaw);
        uint16_t norm = (span == 0) ? 0 : (uint16_t)((uint32_t)(v - minRaw) * 255UL / span);
        uint8_t out = (uint8_t)norm;
        return _dev[dev].invert ? (uint8_t)(255 - out) : out;
    }
    bool getDigital(uint8_t dev, uint8_t ch) const
    {
        if (dev >= _devCount || ch > 15 || _dev[dev].type != DEV_DIGITAL) return false;
        return _dev[dev].d.stable[ch] == HIGH;
    }
    int getValue(uint8_t dev, uint8_t ch) const
    {
        if (dev >= _devCount || ch > 15) return 0;
        if (_dev[dev].type == DEV_ANALOG) return (int)getAnalogFiltered(dev, ch);
        return getDigital(dev, ch) ? 1 : 0;
    }

private:
    struct Device
    {
        DevType type = DEV_ANALOG;
        uint8_t sigPin = 0;
        bool invert = false;
        bool pullup = false;
        uint8_t smoothing = 8; // analog IIR factor
        uint8_t debounce = 3;  // digital samples

        union {
            struct { uint16_t raw[16]; uint16_t filt[16]; bool seeded[16]; } a;
            struct { uint8_t  lastSample[16]; uint8_t stable[16]; uint8_t count[16]; } d;
        };
    };

    void initAnalogBuffers(Device &dev)
    {
        for (uint8_t ch = 0; ch < 16; ++ch) {
            dev.a.raw[ch] = 0; dev.a.filt[ch] = 0; dev.a.seeded[ch] = false;
        }
    }
    void initDigitalBuffers(Device &dev)
    {
        for (uint8_t ch = 0; ch < 16; ++ch) {
            dev.d.lastSample[ch] = LOW; dev.d.stable[ch] = LOW; dev.d.count[ch] = 0;
        }
    }

    inline void _writeAddr(uint8_t ch)
    {
        digitalWrite(_s0, (ch & 0x01) ? HIGH : LOW);
        digitalWrite(_s1, (ch & 0x02) ? HIGH : LOW);
        digitalWrite(_s2, (ch & 0x04) ? HIGH : LOW);
        digitalWrite(_s3, (ch & 0x08) ? HIGH : LOW);
        _lastStepUs = micros(); // start settle timer
    }

    // Bus pins
    uint8_t _s0, _s1, _s2, _s3;

    // Devices
    Device _dev[MAX_DEVICES];
    uint8_t _devCount = 0;

    // Scan state
    uint8_t _currentCh = 0;      // 0..15
    uint16_t _minSettleUs = 10;  // min settle time between address set and sampling
    uint32_t _lastStepUs = 0;
};
