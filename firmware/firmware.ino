// ---------- PIN DEFINITIONS ----------
#define SOIL_PIN       34
#define FLOW_PIN       18
#define CURRENT_PIN    35
#define RELAY_PIN      19

// ---------- RELAY LOGIC ----------
#define PUMP_ON   HIGH
#define PUMP_OFF  LOW
// ---------- SOIL SENSOR CALIBRATION ----------
#define AIR_VALUE      3271
#define WATER_VALUE    1080
// ---------- FLOW SENSOR CALIBRATION ----------
#define PULSES_PER_LITRE 517.0
// ---------------- GLOBAL VARIABLES ----------------
volatile long pulseCount = 0;
// ---------- CURRENT SENSOR CALIBRATION ----------
#define ZERO_OFFSET    2950
#define SENSITIVITY    0.100

// ---------------- FLOW SENSOR INTERRUPT ----------------
void IRAM_ATTR flowISR()
{
    pulseCount++;
}
float readSoilMoisture()
{
    long total = 0;

    // Average 10 readings to reduce noise
    for (int i = 0; i < 10; i++)
    {
        total += analogRead(SOIL_PIN);
        delay(5);
    }

    int rawValue = total / 10;

    float moisture = map(
        rawValue,
        AIR_VALUE,
        WATER_VALUE,
        0,
        100
    );

    moisture = constrain(moisture, 0.0, 100.0);

    return moisture;
}
float readFlowRate()
{
    long startPulses = pulseCount;

    delay(1000);

    long endPulses = pulseCount;

    long pulseDifference = endPulses - startPulses;

    float litresPerSecond = pulseDifference / PULSES_PER_LITRE;

    return litresPerSecond * 60.0;
}
float readCurrent()
{
    long sum = 0;

    // Average 20 ADC samples
    for (int i = 0; i < 20; i++)
    {
        sum += analogRead(CURRENT_PIN);
        delay(5);
    }

    int raw = sum / 20;

    float voltage = (raw / 4095.0) * 3.3;
    float zeroVoltage = (ZERO_OFFSET / 4095.0) * 3.3;

    float current = (voltage - zeroVoltage) / SENSITIVITY;

    return abs(current);
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

void loop()
{
    float soilMoisture = readSoilMoisture();
    float flowRate = readFlowRate();
    float current = readCurrent();
    Serial.print("Soil Moisture : ");
    Serial.print(soilMoisture);
    Serial.println("%");
    Serial.print("Flow Rate     : ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    Serial.print("Current       : ");
    Serial.print(current);
    Serial.println(" A");
    Serial.println("------------------------------");
}