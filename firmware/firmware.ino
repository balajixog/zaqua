#define SOIL_PIN 34
#define FLOW_PIN 18
#define CURRENT_PIN 35
#define RELAY_PIN 19

#define PUMP_ON HIGH
#define PUMP_OFF LOW

#define AIR_VALUE 3271
#define WATER_VALUE 1080

#define PULSES_PER_LITRE 517.0

#define ZERO_OFFSET 2950
#define SENSITIVITY 0.100

#define MOISTURE_ON 40.0
#define MOISTURE_OFF 70.0

#define OVERLOAD_CURRENT 3.5
#define DRY_RUN_FLOW 0.05
#define DRY_RUN_DELAY 5000

#define BLOCKAGE_CURRENT 2.5
#define BLOCKAGE_FLOW 0.30

#define LEAKAGE_MIN_WATER 2.0
#define LEAKAGE_SOIL_GAIN 5.0


volatile long pulseCount = 0;

bool pumpState = false;

int cycleID = 0;
float soilBefore = 0;
float soilAfter = 0;
unsigned long cycleStart = 0;

float totalLitres = 0;

bool faultOverload = false;
bool faultDryRun = false;
bool faultBlockage = false;
bool faultLeakage = false;

bool dryRunTimer = false;
unsigned long dryRunStart = 0;

float avgCurrent = 0.0;
float avgFlow = 0.0;

float currentSum = 0.0;
float flowSum = 0.0;
int sampleCount = 0;

float ECI = 0.0;
float HCI = 0.0;
float SRIt = 0.0;
float EUI = 0.0;

void IRAM_ATTR flowISR()
{
    pulseCount++;
}

float readSoilMoisture()
{
    long total = 0;

    for(int i=0;i<10;i++)
    {
        total += analogRead(SOIL_PIN);
        delay(5);
    }

    int raw = total/10;

    float moisture = map(raw,AIR_VALUE,WATER_VALUE,0,100);
    return constrain(moisture,0,100);
}

float readFlowRate()
{
    long start = pulseCount;
    delay(1000);
    long end = pulseCount;

    float lps = (end-start)/PULSES_PER_LITRE;
    return lps*60.0;
}

float readCurrent()
{
    long total=0;

    for(int i=0;i<20;i++)
    {
        total += analogRead(CURRENT_PIN);
        delay(2);
    }

    float adc = total/20.0;

    float voltage = adc * (3300.0/4095.0);

    float current = (voltage - ZERO_OFFSET)/1000.0;
    current /= SENSITIVITY;

    return abs(current);
}

bool detectLeakage()
{
    float soilGain = soilAfter-soilBefore;

    if(totalLitres>=LEAKAGE_MIN_WATER &&
       soilGain<LEAKAGE_SOIL_GAIN)
    {
        faultLeakage=true;

        Serial.println("=================================");
        Serial.println("WARNING");
        Serial.println("Possible Leakage Detected");
        Serial.print("Water Delivered : ");
        Serial.print(totalLitres);
        Serial.println(" L");
        Serial.print("Soil Gain : ");
        Serial.print(soilGain);
        Serial.println(" %");
        Serial.println("=================================");

        return true;
    }

    return false;
}

void pumpON()
{
    digitalWrite(RELAY_PIN,PUMP_ON);

    pumpState=true;

    cycleID++;
    cycleStart=millis();

    currentSum = 0.0;
    flowSum = 0.0;
    sampleCount = 0;

    totalLitres=0;

    dryRunTimer=false;

    faultOverload=false;
    faultDryRun=false;
    faultBlockage=false;
    faultLeakage=false;

    Serial.println("Cycle Started");
}

void pumpOFF(float currentSoil)
{
    soilAfter=currentSoil;
    calculateKPIs();
    detectLeakage();

    digitalWrite(RELAY_PIN,PUMP_OFF);
    pumpState=false;

    unsigned long duration=millis()-cycleStart;

    detectLeakage();

    Serial.println("=================================");
    Serial.print("Cycle #");
    Serial.println(cycleID);

    Serial.print("Duration : ");
    Serial.print(duration/1000.0);
    Serial.println(" sec");

    Serial.print("Soil Before : ");
    Serial.println(soilBefore);

    Serial.print("Soil After : ");
    Serial.println(soilAfter);

    Serial.print("Water Delivered : ");
    Serial.print(totalLitres);
    Serial.println(" L");

    Serial.println("Pump Status : OFF");
    Serial.println("=================================");
    Serial.println("----------- KPIs -----------");

    Serial.print("Average Current : ");
    Serial.print(avgCurrent);
    Serial.println(" A");

    Serial.print("Average Flow : ");
    Serial.print(avgFlow);
    Serial.println(" L/min");

    Serial.print("ECI : ");
    Serial.println(ECI);

    Serial.print("HCI : ");
    Serial.println(HCI);

    Serial.print("SRIt : ");
    Serial.println(SRIt);

    Serial.print("EUI : ");
    Serial.println(EUI);

    Serial.println("----------------------------");
}

bool detectOverload(float current)
{
    if(current>OVERLOAD_CURRENT)
    {
        faultOverload=true;
        dryRunTimer=false;

        Serial.println("FAULT : OVERLOAD");
        pumpOFF(readSoilMoisture());

        return true;
    }

    return false;
}

bool detectDryRun(float flow)
{
    if(flow<DRY_RUN_FLOW)
    {
        if(!dryRunTimer)
        {
            dryRunTimer=true;
            dryRunStart=millis();
        }
        else if(millis()-dryRunStart>=DRY_RUN_DELAY)
        {
            faultDryRun=true;

            Serial.println("FAULT : DRY RUN");
            pumpOFF(readSoilMoisture());

            return true;
        }
    }
    else
    {
        dryRunTimer=false;
    }

    return false;
}

bool detectBlockage(float current,float flow)
{
    if(current>=BLOCKAGE_CURRENT &&
       flow<BLOCKAGE_FLOW)
    {
        faultBlockage=true;

        Serial.println("WARNING : Possible Blockage");

        return true;
    }

    faultBlockage=false;

    return false;
}
void calculateKPIs()
{
    if (sampleCount == 0)
        return;

    avgCurrent = currentSum / sampleCount;
    avgFlow = flowSum / sampleCount;

    float soilGain = soilAfter - soilBefore;

    // Electrical Conversion Indicator
    if (avgCurrent > 0)
        ECI = avgFlow / avgCurrent;
    else
        ECI = 0;

    // Hydraulic Consistency Indicator
    HCI = avgFlow;

    // Soil Recovery Indicator
    SRIt = soilGain;

    // Energy Utilization Indicator
    if (avgCurrent > 0)
        EUI = soilGain / avgCurrent;
    else
        EUI = 0;
}

void setup()
{
    Serial.begin(115200);

    pinMode(SOIL_PIN,INPUT);
    pinMode(CURRENT_PIN,INPUT);
    pinMode(FLOW_PIN,INPUT_PULLUP);
    pinMode(RELAY_PIN,OUTPUT);

    digitalWrite(RELAY_PIN,PUMP_OFF);

    attachInterrupt(digitalPinToInterrupt(FLOW_PIN),flowISR,RISING);
}

void loop()
{
    float soil=readSoilMoisture();
    float flow=readFlowRate();
    float current=readCurrent();

    if(pumpState)
    {
        currentSum += current;
        flowSum += flow;
        sampleCount++;
        totalLitres += flow/60.0;

        if(detectOverload(current))
            return;

        if(detectDryRun(flow))
            return;

        detectBlockage(current,flow);
    }

    if(!pumpState && soil<MOISTURE_ON)
    {
        soilBefore=soil;
        pumpON();
    }

    if(pumpState && soil>=MOISTURE_OFF)
    {
        pumpOFF(soil);
    }

    delay(1000);
}
