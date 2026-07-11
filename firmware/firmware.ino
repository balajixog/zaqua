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
bool pumpState = false;
// ---------- IRRIGATION CYCLE ----------
int cycleID = 0;
// ---------- FAULT THRESHOLDS ----------
#define OVERLOAD_CURRENT 3.5
#define DRY_RUN_FLOW    0.05
#define DRY_RUN_DELAY   5000

float soilBefore = 0;
float soilAfter = 0;
unsigned long cycleStart = 0;
bool faultOverload = false;
bool faultDryRun = false;

bool dryRunTimer = false;
unsigned long dryRunStart = 0;
// ---------- CURRENT SENSOR CALIBRATION ----------
#define ZERO_OFFSET    2950
#define SENSITIVITY    0.100
// ---------- IRRIGATION THRESHOLDS ----------
#define MOISTURE_ON     40.0
#define MOISTURE_OFF    70.0

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
bool detectOverload(float current)
{
    if (current > OVERLOAD_CURRENT)
    {
        faultOverload = true;

        Serial.println("=================================");
        Serial.println("FAULT DETECTED");
        Serial.println("Reason : OVERLOAD");
        Serial.println("Pump stopped to protect hardware.");
        Serial.println("=================================");

        pumpOFF();

        return true;
    }

    return false;
}
bool detectDryRun(float flowRate)
{
    if (flowRate < DRY_RUN_FLOW)
    {
        if (!dryRunTimer)
        {
            dryRunTimer = true;
            dryRunStart = millis();
        }
        else if (millis() - dryRunStart >= DRY_RUN_DELAY)
        {
            faultDryRun = true;

            Serial.println("=================================");
            Serial.println("FAULT DETECTED");
            Serial.println("Reason : DRY RUN");
            Serial.println("No water flow detected.");
            Serial.println("Pump stopped to prevent damage.");
            Serial.println("=================================");

            pumpOFF();

            return true;
        }
    }
    else
    {
        dryRunTimer = false;
    }

    return false;
}
void pumpON()
{
    digitalWrite(RELAY_PIN, PUMP_ON);
    pumpState = true;
    dryRunTimer = false;
    faultDryRun = false;

    cycleID++;
    cycleStart = millis();

    Serial.println("=================================");
    Serial.print("Cycle #");
    Serial.print(cycleID);
    Serial.println(" Started");

    Serial.print("Start Time : ");
    Serial.print(cycleStart);
    Serial.println(" ms");

    Serial.println("Pump Status : ON");
    Serial.println("=================================");
}

void pumpOFF()
{
    digitalWrite(RELAY_PIN, PUMP_OFF);
    pumpState = false;

    unsigned long duration = millis() - cycleStart;

    Serial.println("=================================");
    Serial.print("Cycle #");
    Serial.print(cycleID);
    Serial.println(" Completed");

    Serial.print("Duration : ");
    Serial.print(duration / 1000.0);
    Serial.println(" sec");

    Serial.print("Soil Before : ");
    Serial.print(soilBefore);
    Serial.println("%");

    Serial.print("Soil After : ");
    Serial.print(soilAfter);
    Serial.println("%");

    Serial.println("Pump Status : OFF");
    Serial.println("=================================");
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
    if (pumpState)
    {
        if (detectOverload(current))
        {
            delay(1000);
            return;
        }
    
        if (detectDryRun(flowRate))
        {
            delay(1000);
            return;
        }
    }

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
    // Automatic Irrigation Control
    if (!pumpState && soilMoisture < MOISTURE_ON)
    {
        soilBefore = soilMoisture;
        pumpON();
    }
    
    if (pumpState && soilMoisture >= MOISTURE_OFF)
    {
        soilAfter = soilMoisture;
        pumpOFF();
    }
    
    delay(1000);
}