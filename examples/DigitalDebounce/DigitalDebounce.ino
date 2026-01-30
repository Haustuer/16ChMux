#include <Mux16Ch.h>

constexpr uint8_t S0 = 2, S1 = 3, S2 = 4, S3 = 5;
BusMUX16<2> mux(S0, S1, S2, S3);

void setup() {
  Serial.begin(115200);
  while (!Serial) { }

  mux.addDigitalDevice(7, /*pullup=*/true, /*invert=*/false, /*debounce=*/5);
  mux.addDigitalDevice(8, /*pullup=*/true, /*invert=*/true,  /*debounce=*/5);
  mux.setup();
}

void loop() {
  mux.loop();

  static uint32_t last = 0;
  if (millis() - last > 500) {
    last = millis();
    for (uint8_t ch=0; ch<16; ++ch) {
      bool a = mux.getDigital(0, ch);
      bool b = mux.getDigital(1, ch);
      Serial.print(ch); Serial.print(": ");
      Serial.print(a);  Serial.print(" ");
      Serial.println(b);
    }
    Serial.println();
  }
}
