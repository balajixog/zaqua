// ---------- PIN DEFINITIONS ----------
#define SOIL_PIN       34
#define FLOW_PIN       18
#define CURRENT_PIN    35
#define RELAY_PIN      19

// ---------- RELAY LOGIC ----------
#define PUMP_ON   HIGH
#define PUMP_OFF  LOW
// ---------------- GLOBAL VARIABLES ----------------
volatile long pulseCount = 0;

// ---------------- FLOW SENSOR INTERRUPT ----------------
void IRAM_ATTR flowISR()
{
    pulseCount++;
}
void setup()
{
    Serial.begin(115200);
    delay(500);

    // Configure GPIO
    pinMode(SOIL_PIN, INPUT);
    pinMode(CURRENT_PIN, INPUT);
    pinMode(FLOW_PIN, INPUT_PULLUP);

    pinMode(RELAY_PIN, OUTPUT);

    // Keep pump OFF at startup
    digitalWrite(RELAY_PIN, PUMP_OFF);

    // Configure flow sensor interrupt
    attachInterrupt(
        digitalPinToInterrupt(FLOW_PIN),
        flowISR,
        RISING
    );

    Serial.println();
    Serial.println("==================================");
    Serial.println(" HybridFlow Firmware");
    Serial.println(" Hardware Initialized");
    Serial.println(" ESP32 Ready");
    Serial.println("==================================");
}

void loop() {

}