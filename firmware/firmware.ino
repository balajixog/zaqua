#define SOIL_PIN       34
#define FLOW_PIN       18
#define CURRENT_PIN    35
#define RELAY_PIN      19

#define PUMP_ON HIGH
#define PUMP_OFF LOW

//===================== CALIBRATION =====================

#define AIR_VALUE      3271
#define WATER_VALUE    1080

#define PULSES_PER_LITRE 517.0

// ACS712 (20A)
#define ZERO_OFFSET    2950
#define SENSITIVITY    0.100

//===================== CONTROL =====================

#define MOISTURE_ON    40.0
#define MOISTURE_OFF   70.0

//===================== FAULT LIMITS =====================

#define OVERLOAD_CURRENT   3.5

#define DRY_RUN_FLOW       0.05
#define DRY_RUN_DELAY      5000

#define BLOCKAGE_CURRENT   2.5
#define BLOCKAGE_FLOW      0.30

#define LEAKAGE_MIN_WATER  2.0
#define LEAKAGE_SOIL_GAIN  5.0

//===================== GLOBALS =====================

volatile long pulseCount = 0;

bool pumpState = false;

int cycleID = 0;

float soilBefore = 0;
float soilAfter = 0;

unsigned long cycleStart = 0;

float totalLitres = 0;

//---------------- Fault Flags ----------------

bool faultOverload = false;
bool faultDryRun = false;
bool faultBlockage = false;
bool faultLeakage = false;

bool dryRunTimer = false;
unsigned long dryRunStart = 0;

String faultName = "NONE";

//---------------- Statistics ----------------

float currentSum = 0;
float flowSum = 0;

int sampleCount = 0;

float avgCurrent = 0;
float avgFlow = 0;

//---------------- KPIs ----------------

float ECI = 0;
float HCI = 0;
float SRIt = 0;
float EUI = 0;

float ECI_norm = 0;
float HCI_norm = 0;
float SRIt_norm = 0;
float EUI_norm = 0;

float DIIS = 0;

//===================== PROTOTYPES =====================

void pumpON();
void pumpOFF(float soil);

float readSoilMoisture();
float readCurrent();
float readFlowRate();

bool detectOverload(float current);
bool detectDryRun(float flow);
bool detectBlockage(float current,float flow);
bool detectLeakage();

void calculateKPIs();
void calculateDIIS();

void exportCSV();
void printCycleSummary();

bool checkFaults(float current,float flow);

float normalize(float value,float min,float max);
String getDIISStatus();
String getRecommendation();
void resetStatistics();

//===================== ISR =====================

void IRAM_ATTR flowISR()
{
    pulseCount++;
}

//===================== SENSOR FUNCTIONS =====================

float readSoilMoisture()
{
    long total = 0;

    for(int i=0;i<10;i++)
    {
        total += analogRead(SOIL_PIN);
        delay(5);
    }

    int raw = total / 10;

    float moisture =
        map(raw,
            AIR_VALUE,
            WATER_VALUE,
            0,
            100);

    return constrain(moisture,0,100);
}

float readCurrent()
{
    long total = 0;

    for(int i=0;i<20;i++)
    {
        total += analogRead(CURRENT_PIN);
        delay(2);
    }

    float adc = total / 20.0;

    float voltage =
        adc * (3300.0 / 4095.0);

    float current =
        (voltage - ZERO_OFFSET) / 1000.0;

    current /= SENSITIVITY;

    return abs(current);
}

float readFlowRate()
{
    long start = pulseCount;

    delay(1000);

    long end = pulseCount;

    float litresPerSecond =
        (end - start) / PULSES_PER_LITRE;

    return litresPerSecond * 60.0;
}

//====================================================
// PUMP CONTROL
//====================================================

void pumpON()
{
    digitalWrite(RELAY_PIN, PUMP_ON);

    pumpState = true;
    faultName = "NONE";

    cycleID++;
    cycleStart = millis();

    soilAfter = 0;
    totalLitres = 0;

    currentSum = 0;
    flowSum = 0;
    sampleCount = 0;

    faultOverload = false;
    faultDryRun = false;
    faultBlockage = false;
    faultLeakage = false;

    dryRunTimer = false;

    Serial.println();
    Serial.println("==================================");
    Serial.print("Cycle #");
    Serial.print(cycleID);
    Serial.println(" Started");
    Serial.println("==================================");
}

void pumpOFF(float currentSoil)
{
    digitalWrite(RELAY_PIN, PUMP_OFF);
    pumpState = false;

    soilAfter = currentSoil;

    calculateKPIs();
    calculateDIIS();

    detectLeakage();

    exportCSV();

    printCycleSummary();
}

//====================================================
// OVERLOAD
//====================================================

bool detectOverload(float current)
{
    if(current > OVERLOAD_CURRENT)
    {
        faultOverload = true;
        faultName = "OVERLOAD";

        Serial.println();
        Serial.println("FAULT : OVERLOAD");

        pumpOFF(readSoilMoisture());

        return true;
    }

    return false;
}

//====================================================
// DRY RUN
//====================================================

bool detectDryRun(float flow)
{
    if(flow < DRY_RUN_FLOW)
    {
        if(!dryRunTimer)
        {
            dryRunTimer = true;
            dryRunStart = millis();
        }
        else if(millis() - dryRunStart >= DRY_RUN_DELAY)
        {
            faultDryRun = true;
            faultName = "DRY_RUN";

            Serial.println();
            Serial.println("FAULT : DRY RUN");

            pumpOFF(readSoilMoisture());

            return true;
        }
    }
    else
    {
        dryRunTimer = false;
    }

    return false;
}

//====================================================
// BLOCKAGE
//====================================================

bool detectBlockage(float current,float flow)
{
    if(current >= BLOCKAGE_CURRENT &&
       flow < BLOCKAGE_FLOW)
    {
        faultBlockage = true;
        faultName = "BLOCKAGE";

        Serial.println();
        Serial.println("WARNING : BLOCKAGE DETECTED");

        return true;
    }

    faultBlockage = false;

    return false;
}

//====================================================
// LEAKAGE
//====================================================

bool detectLeakage()
{
    float soilGain = soilAfter - soilBefore;

    if(totalLitres >= LEAKAGE_MIN_WATER &&
       soilGain < LEAKAGE_SOIL_GAIN)
    {
        faultLeakage = true;
        faultName = "LEAKAGE";

        Serial.println();
        Serial.println("WARNING : POSSIBLE LEAKAGE");
        Serial.print("Water Delivered : ");
        Serial.print(totalLitres);
        Serial.println(" L");

        Serial.print("Soil Gain : ");
        Serial.print(soilGain);
        Serial.println(" %");

        return true;
    }

    return false;
}

//====================================================
// CENTRALIZED FAULT CHECK
//====================================================

bool checkFaults(float current,float flow)
{
    if(detectOverload(current))
        return true;

    if(detectDryRun(flow))
        return true;

    detectBlockage(current,flow);

    return false;
}

//====================================================
// NORMALIZATION
//====================================================

float normalize(float value, float minValue, float maxValue)
{
    if (value <= minValue)
        return 0.0;

    if (value >= maxValue)
        return 100.0;

    return ((value - minValue) * 100.0) /
           (maxValue - minValue);
}

//====================================================
// KPI CALCULATION
//====================================================

void calculateKPIs()
{
    if (sampleCount <= 0)
    {
        avgCurrent = 0;
        avgFlow = 0;

        ECI = 0;
        HCI = 0;
        SRIt = 0;
        EUI = 0;

        return;
    }

    avgCurrent = currentSum / sampleCount;
    avgFlow = flowSum / sampleCount;

    float soilGain = soilAfter - soilBefore;

    //------------------------------------------------
    // ECI
    //------------------------------------------------

    if (avgCurrent > 0)
        ECI = avgFlow / avgCurrent;
    else
        ECI = 0;

    //------------------------------------------------
    // HCI
    //------------------------------------------------

    HCI = avgFlow;

    //------------------------------------------------
    // SRIt
    //------------------------------------------------

    SRIt = soilGain;

    //------------------------------------------------
    // EUI
    //------------------------------------------------

    if (avgCurrent > 0)
        EUI = soilGain / avgCurrent;
    else
        EUI = 0;
}

//====================================================
// DIIS CALCULATION
//====================================================

void calculateDIIS()
{
    //------------------------------------------------
    // Normalize KPIs
    //------------------------------------------------

    ECI_norm = normalize(ECI, 0.0, 5.0);

    HCI_norm = normalize(HCI, 0.0, 5.0);

    SRIt_norm = normalize(SRIt, 0.0, 40.0);

    EUI_norm = normalize(EUI, 0.0, 50.0);

    //------------------------------------------------
    // Weighted DIIS
    //------------------------------------------------

    DIIS =
        (ECI_norm * 0.25) +
        (HCI_norm * 0.25) +
        (SRIt_norm * 0.25) +
        (EUI_norm * 0.25);

    //------------------------------------------------
    // Limit score
    //------------------------------------------------

    DIIS = constrain(DIIS, 0.0, 100.0);
}

//====================================================
// DIIS STATUS
//====================================================

String getDIISStatus()
{
    if (DIIS >= 85)
        return "EXCELLENT";

    if (DIIS >= 70)
        return "GOOD";

    if (DIIS >= 50)
        return "FAIR";

    return "POOR";
}

//====================================================
// KPI RESET
//====================================================

void resetStatistics()
{
    currentSum = 0;
    flowSum = 0;

    sampleCount = 0;

    avgCurrent = 0;
    avgFlow = 0;

    totalLitres = 0;

    ECI = 0;
    HCI = 0;
    SRIt = 0;
    EUI = 0;

    ECI_norm = 0;
    HCI_norm = 0;
    SRIt_norm = 0;
    EUI_norm = 0;

    DIIS = 0;
}

//====================================================
// CSV EXPORT
//====================================================

void exportCSV()
{
    float duration =
        (millis() - cycleStart) / 1000.0;

    Serial.print(cycleID);
    Serial.print(",");

    Serial.print(duration);
    Serial.print(",");

    Serial.print(soilBefore);
    Serial.print(",");

    Serial.print(soilAfter);
    Serial.print(",");

    Serial.print(avgCurrent);
    Serial.print(",");

    Serial.print(avgFlow);
    Serial.print(",");

    Serial.print(totalLitres);
    Serial.print(",");

    Serial.print(ECI);
    Serial.print(",");

    Serial.print(HCI);
    Serial.print(",");

    Serial.print(SRIt);
    Serial.print(",");

    Serial.print(EUI);
    Serial.print(",");

    Serial.print(ECI_norm);
    Serial.print(",");

    Serial.print(HCI_norm);
    Serial.print(",");

    Serial.print(SRIt_norm);
    Serial.print(",");

    Serial.print(EUI_norm);
    Serial.print(",");

    Serial.print(DIIS);
    Serial.print(",");

    Serial.println(faultName);
}

//====================================================
// RECOMMENDATION
//====================================================

String getRecommendation()
{
    if (faultName == "NONE")
        return "System operating normally.";

    if (faultName == "OVERLOAD")
        return "Inspect motor and electrical wiring.";

    if (faultName == "DRY_RUN")
        return "Check water source or inlet.";

    if (faultName == "BLOCKAGE")
        return "Inspect outlet pipe or filter.";

    if (faultName == "LEAKAGE")
        return "Inspect irrigation pipeline.";

    return "Unknown condition.";
}

//====================================================
// IRRIGATION REPORT
//====================================================

void printCycleSummary()
{
    float duration =
        (millis() - cycleStart) / 1000.0;

    Serial.println();
    Serial.println("================================================");
    Serial.println("          HYBRIDFLOW IRRIGATION REPORT");
    Serial.println("================================================");

    Serial.print("Cycle ID           : ");
    Serial.println(cycleID);

    Serial.print("Duration           : ");
    Serial.print(duration);
    Serial.println(" sec");

    Serial.println();

    //------------------------------------------------
    // Soil
    //------------------------------------------------

    Serial.println("--------------- SOIL ----------------");

    Serial.print("Before             : ");
    Serial.print(soilBefore);
    Serial.println(" %");

    Serial.print("After              : ");
    Serial.print(soilAfter);
    Serial.println(" %");

    Serial.print("Improvement        : ");
    Serial.print(soilAfter - soilBefore);
    Serial.println(" %");

    Serial.println();

    //------------------------------------------------
    // Water
    //------------------------------------------------

    Serial.println("-------------- WATER ----------------");

    Serial.print("Average Flow       : ");
    Serial.print(avgFlow);
    Serial.println(" L/min");

    Serial.print("Total Water        : ");
    Serial.print(totalLitres);
    Serial.println(" L");

    Serial.println();

    //------------------------------------------------
    // Electrical
    //------------------------------------------------

    Serial.println("------------ ELECTRICAL -------------");

    Serial.print("Average Current    : ");
    Serial.print(avgCurrent);
    Serial.println(" A");

    Serial.println();

    //------------------------------------------------
    // KPIs
    //------------------------------------------------

    Serial.println("---------------- KPIs ---------------");

    Serial.print("ECI                : ");
    Serial.println(ECI);

    Serial.print("HCI                : ");
    Serial.println(HCI);

    Serial.print("SRIt               : ");
    Serial.println(SRIt);

    Serial.print("EUI                : ");
    Serial.println(EUI);

    Serial.println();

    //------------------------------------------------
    // Normalized KPIs
    //------------------------------------------------

    Serial.println("---------- NORMALIZED KPIs ----------");

    Serial.print("ECI Norm           : ");
    Serial.println(ECI_norm);

    Serial.print("HCI Norm           : ");
    Serial.println(HCI_norm);

    Serial.print("SRIt Norm          : ");
    Serial.println(SRIt_norm);

    Serial.print("EUI Norm           : ");
    Serial.println(EUI_norm);

    Serial.println();

    //------------------------------------------------
    // DIIS
    //------------------------------------------------

    Serial.println("---------------- DIIS ---------------");

    Serial.print("Score              : ");
    Serial.println(DIIS);

    Serial.print("Status             : ");
    Serial.println(getDIISStatus());

    Serial.println();

    //------------------------------------------------
    // Fault
    //------------------------------------------------

    Serial.println("---------------- FAULT --------------");

    Serial.print("Detected           : ");
    Serial.println(faultName);

    Serial.print("Recommendation     : ");
    Serial.println(getRecommendation());

    Serial.println();

    //------------------------------------------------
    // Overall Health
    //------------------------------------------------

    Serial.println("------------- SYSTEM ----------------");

    if(faultName == "NONE" && DIIS >= 85)
    {
        Serial.println("Overall Health : EXCELLENT");
    }
    else if(faultName == "NONE" && DIIS >= 70)
    {
        Serial.println("Overall Health : GOOD");
    }
    else if(DIIS >= 50)
    {
        Serial.println("Overall Health : FAIR");
    }
    else
    {
        Serial.println("Overall Health : NEEDS ATTENTION");
    }

    Serial.println("================================================");
    Serial.println();
}
//===================== SETUP =====================

void setup()
{
    Serial.begin(115200);

    pinMode(SOIL_PIN,INPUT);
    pinMode(CURRENT_PIN,INPUT);

    pinMode(FLOW_PIN,INPUT_PULLUP);

    pinMode(RELAY_PIN,OUTPUT);

    digitalWrite(RELAY_PIN,PUMP_OFF);

    attachInterrupt(
        digitalPinToInterrupt(FLOW_PIN),
        flowISR,
        RISING);

    Serial.println();

    Serial.println(
    "cycle_id,duration_sec,"
    "soil_before,"
    "soil_after,"
    "avg_current,"
    "avg_flow,"
    "total_litres,"
    "ECI,HCI,SRIt,EUI,"
    "ECI_norm,HCI_norm,"
    "SRIt_norm,EUI_norm,"
    "DIIS,fault");
}
//====================================================
// MAIN LOOP
//====================================================

void loop()
{
    //------------------------------------------
    // Read Sensors
    //------------------------------------------

    float soilMoisture = readSoilMoisture();
    float flowRate     = readFlowRate();
    float current      = readCurrent();

    //------------------------------------------
    // Pump Running
    //------------------------------------------

    if (pumpState)
    {
        currentSum += current;
        flowSum += flowRate;
        sampleCount++;

        // Water delivered in this cycle
        totalLitres += flowRate / 60.0;

        // Fault Monitoring
        if (checkFaults(current, flowRate))
        {
            delay(500);
            return;
        }
    }

    //------------------------------------------
    // Live Sensor Display
    //------------------------------------------

    Serial.println("----------------------------------------");

    Serial.print("Soil Moisture : ");
    Serial.print(soilMoisture);
    Serial.println(" %");

    Serial.print("Flow Rate     : ");
    Serial.print(flowRate);
    Serial.println(" L/min");

    Serial.print("Current       : ");
    Serial.print(current);
    Serial.println(" A");

    Serial.print("Pump Status   : ");

    if (pumpState)
        Serial.println("ON");
    else
        Serial.println("OFF");

    Serial.println("----------------------------------------");
    Serial.println();

    //------------------------------------------
    // Automatic Irrigation Start
    //------------------------------------------

    if (!pumpState &&
        soilMoisture <= MOISTURE_ON)
    {
        soilBefore = soilMoisture;

        resetStatistics();

        pumpON();
    }

    //------------------------------------------
    // Automatic Irrigation Stop
    //------------------------------------------

    if (pumpState &&
        soilMoisture >= MOISTURE_OFF)
    {
        pumpOFF(soilMoisture);
    }

    //------------------------------------------
    // Refresh Rate
    //------------------------------------------

    delay(1000);
}