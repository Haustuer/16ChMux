# BusMUX16

A non-blocking multi-multiplexer manager that shares S0..S3 across multiple 16-channel analog/digital mux chips (e.g., 74HC4067). Each chip has its own SIG pin; the class scans channels 0..15 and maintains per-channel buffers.

- Header-only, **template** parameter `MAX_DEVICES` for memory-optimal builds
- Analog IIR smoothing per channel
- Digital sample-based debounce per channel
- Deterministic, timer-guarded settling after address changes

## Installation
- **Arduino IDE**: Sketch → Include Library → Add .ZIP Library… (use the release ZIP)
- **PlatformIO**: add Git URL (or release tag) to `lib_deps`

## Quick Start
```cpp
#include <BusMUX16.h>
BusMUX16<3> mux(2,3,4,5);
void setup(){
  mux.addAnalogDevice(A0); // first device index 0
  mux.addDigitalDevice(6); // second device index 1
  mux.setup();
}
void loop(){
  mux.loop();
  uint16_t a = mux.getAnalogFiltered(0, 0); // device 0, ch 0
  bool d = mux.getDigital(1, 0);            // device 1, ch 0
}
```

## API Highlights
- `addAnalogDevice(sigPin, invert=false, smoothing=8)`
- `addDigitalDevice(sigPin, pullup=true, invert=false, debounceSamples=3)`
- `setMinSettleMicros(us)`
- `getAnalogRaw(dev, ch)`, `getAnalogFiltered(dev, ch)`, `getAnalog8(dev, ch, min, max)`
- `getDigital(dev, ch)`
- `getValue(dev, ch)` (returns 0..1023 filtered analog or 0/1 for digital)

## Notes
- Default ADC mapping uses 10-bit (0..1023). For 12-bit MCUs, pass `maxRaw=4095` to `getAnalog8`.
- For high-impedance sources, consider adding a dummy read after address change (future enhancement).
