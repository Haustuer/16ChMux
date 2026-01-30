#include <Mux16Ch.h>

// Shared address lines
constexpr uint8_t S0 = 2, S1 = 3, S2 = 4, S3 = 5;

// Reserve memory for up to 3 mux chips
BusMUX16<3> mux(S0, S1, S2, S3);

void setup() {
  Serial.begin(115200);
  while (!Serial) { }

  uint8_t aDev = mux.addAnalogDevice(A0, /*invert=*/false, /*smoothing=*/8);
  uint8_t dDev = mux.addDigitalDevice(6,  /*pullup=*/true,  /*invert=*/false, /*debounce=*/3);
  (void)aDev; (void)dDev;

  mux.setMinSettleMicros(10);
  mux.setup();
}

void loop() {
  mux.loop();

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 250) {
    lastPrint = millis();
    Serial.print("A0 ch0 filt: ");
    Serial.print(mux.getAnalogFiltered(0, 0));
    Serial.print("  D6 ch0: ");
    Serial.println(mux.getDigital(1, 0) ? "HIGH" : "LOW");
  }
}
