#include <SoftwareSerial.h>
#include <arduino-timer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <xbee_zha.h>
#include "zha/device_details.h"

#define TEMP_BUS 5
#define XBEE_RST 4

// Define SoftSerial TX/RX pins
#define ssRX 10
#define ssTX 11

#define BASIC_ENDPOINT 1
#define DOOR1_ENDPOINT 2
#define DOOR2_ENDPOINT 3
#define DOOR3_ENDPOINT 4
#define DOOR4_ENDPOINT 5
#define TEMP_ENDPOINT 6
#define MAX_TEMP_ATTEMPTS 10

#define START_LOOPS 100

uint8_t start_fails = 0;
uint8_t init_status_sent = 0;

constexpr uint8_t DOOR_OUT_PINS[] = {12, 13, A0, A1};
constexpr uint8_t DOOR_IN_PINS[] = {9, 8, 7, 6};

void (*resetFunc)(void) = 0;

auto timer = timer_create_default(); // create a timer with default settings

unsigned long loop_time = millis();
unsigned long last_msg_time = loop_time - 1000;

// One wire temp sensors
OneWire oneWire(TEMP_BUS);
DallasTemperature sensors(&oneWire);
// Input Temp
DeviceAddress devThermometer = {0x28, 0x2E, 0x53, 0x45, 0x92, 0x17, 0x02, 0x58};

SoftwareSerial nss(ssRX, ssTX);

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char *sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif // __arm__

int freeMemory()
{
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char *>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif // __arm__
}

void setup()
{
  pinMode(XBEE_RST, OUTPUT);

  // Reset xbee
  digitalWrite(XBEE_RST, LOW);
  pinMode(XBEE_RST, INPUT);

  // Configure Door Pins
  for (uint8_t i = 0; i < 4; i++)
  {
    pinMode(DOOR_OUT_PINS[i], OUTPUT);
    digitalWrite(DOOR_OUT_PINS[i], LOW);
    pinMode(DOOR_IN_PINS[i], INPUT);
  }

  Serial.begin(115200);
  Serial.println(F("Startup"));
  sensors.begin();
  nss.begin(9600);
  uint8_t temp_attempts = 0;

  bool success = 0;
  while (!success && temp_attempts < MAX_TEMP_ATTEMPTS)
  {
    Serial.print(F("Found "));
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(F(" Temp Dev."));

    success = sensors.getAddress(devThermometer, 0);
    temp_attempts++;
    delay(100);
  }

  if (success)
  {
    Serial.print(F("Dev Addr: "));
    for (int i; i < sizeof(devThermometer); i++)
    {
      Serial.print(devThermometer[i]);
      Serial.print(F(" "));
    }
    Serial.println();
    // We really need this to happen, otherwise get odd results
    sensors.setResolution(devThermometer, 9);
  }
  else
  {
    Serial.println(F("T Dev Addr Fail"));
  }

  zha.Start(nss, zhaClstrCmd, zhaWriteAttr, NUM_ENDPOINTS, ENDPOINTS);

  // Set up callbacks, shouldn't have to do this here, but bad design...
  zha.registerCallbacks(atCmdResp, zbTxStatusResp, otherResp, zdoReceive);

  Serial.println(F("CB Conf"));

  timer.every(30000, update_sensors);
}

bool read_temp(DeviceAddress thermometer, int16_t *temp)
{
  bool success = sensors.requestTemperaturesByAddress(thermometer);

  if (!success)
  {
    Serial.println(F("T addr not found"));
    return false;
  }

  delay(10); // No idea why this is needed

  float TempC = sensors.getTempC(thermometer);
  if (TempC == DEVICE_DISCONNECTED_C)
  {
    Serial.println(F("T Disconnect"));
    return false;
  }

  Serial.print(F("Temp: "));
  Serial.print(String(TempC, 2));
  Serial.println(F("°C"));

  *temp = (int16_t)(TempC * 100.0);
  return true;
}

void update_temp()
{
  // Temp
  int16_t res_temp;
  bool r_success = read_temp(devThermometer, &res_temp);

  if (r_success && zha.dev_status == READY)
  {
    Endpoint end_point = zha.GetEndpoint(TEMP_ENDPOINT);
    Cluster cluster = end_point.GetCluster(TEMP_CLUSTER_ID);
    attribute *attr;
    uint8_t attr_exists = cluster.GetAttr(&attr, CURRENT_STATE);
    attr->SetValue(res_temp);
    zha.sendAttributeRpt(cluster.id, attr, end_point.id, 1);
  }
}

bool update_sensors(void *)
{
  update_temp();
  return true;
}

void update_door_state(uint8_t force)
{
  for (uint8_t i = 0; i < 4; i++)
  {
    uint8_t val = digitalRead(DOOR_IN_PINS[i]) ^ 1;
    Endpoint end_point = zha.GetEndpoint(i + 2);
    Cluster cluster = end_point.GetCluster(ON_OFF_CLUSTER_ID);
    attribute *attr;
    uint8_t attr_exists = cluster.GetAttr(&attr, CURRENT_STATE);

    if ((val != attr->GetIntValue(0x00)) || force)
    {
      Serial.print(F("EP"));
      Serial.print(end_point.id);
      Serial.print(F(": "));
      Serial.print(attr->GetIntValue(0x00));
      Serial.print(F(" Now "));
      attr->SetValue(val);
      Serial.println(attr->GetIntValue(0x00));
      zha.sendAttributeRpt(cluster.id, attr, end_point.id, 1);
    }
  }
}

void SetAttr(uint8_t ep_id, uint16_t cluster_id, uint16_t attr_id, uint8_t value, uint8_t rqst_seq_id)
{
  Endpoint end_point = zha.GetEndpoint(ep_id);
  Cluster cluster = end_point.GetCluster(cluster_id);
  attribute *attr;
  uint8_t attr_exists = cluster.GetAttr(&attr, attr_id);

  Serial.print("Clstr: ");
  Serial.println(cluster_id, HEX);

  if (cluster_id == ON_OFF_CLUSTER_ID)
  {
    // We don't want to set value here, value is set by the door opening or closing
    if (value == 0x00 || 0x01)
    {
      Serial.print(F("Toggle: "));
      Serial.println(end_point.id);
      digitalWrite(DOOR_OUT_PINS[end_point.id - 1], HIGH);
      zha.sendAttributeCmdRsp(cluster_id, attr, ep_id, 1, value, zha.cmd_seq_id); // Tell sender that we did what we were told to
      delay(2000);
      digitalWrite(DOOR_OUT_PINS[end_point.id - 1], LOW);
    }
  }
}

void loop()
{
  zha.loop();

  if (zha.dev_status == READY)
  {
    if (!init_status_sent)
    {
      Serial.println(F("Snd Init States"));
      update_door_state(0x01);
      init_status_sent = 1;
    }
    else
    {
      update_door_state(0x00);
      if ((loop_time - last_msg_time) > 1000)
      {
        Serial.print(F("Mem: "));
        Serial.println(freeMemory());
        last_msg_time = millis();
      }
    }
  }
  else if ((loop_time - last_msg_time) > 1000)
  {
    Serial.print(F("Not Started "));
    Serial.print(start_fails);
    Serial.print(F(" of "));
    Serial.println(START_LOOPS);

    last_msg_time = millis();
    if (start_fails == 15)
    {
      // Sometimes we don't get a response from dev ann, try a transmit and see if we are good
      update_door_state(0x01);
    }
    if (start_fails > START_LOOPS)
    {
      resetFunc();
    }
    start_fails++;
  }

  timer.tick();
  loop_time = millis();
}
void zhaWriteAttr(ZBExplicitRxResponse &erx)
{
  // Not used
  Serial.println(F("Write Cmd"));
}

void zhaClstrCmd(ZBExplicitRxResponse &erx)
{
  Serial.println(F("Clstr Cmd"));
  if ((erx.getDstEndpoint() == DOOR1_ENDPOINT) ||
      (erx.getDstEndpoint() == DOOR2_ENDPOINT) ||
      (erx.getDstEndpoint() == DOOR3_ENDPOINT) ||
      (erx.getDstEndpoint() == DOOR4_ENDPOINT))
  {
    Serial.println(F("Door Ep"));
    if (erx.getClusterId() == ON_OFF_CLUSTER_ID)
    {
      Serial.println(F("ON/OFF"));
      Endpoint end_point = zha.GetEndpoint(erx.getDstEndpoint());
      uint8_t len_data = erx.getDataLength() - 3;
      uint8_t new_state = erx.getFrameData()[erx.getDataOffset() + 2];
      uint16_t attr_rqst[len_data / 2];

      for (uint8_t i = erx.getDataOffset(); i < (erx.getDataLength() + erx.getDataOffset() + 3); i++)
      {
        Serial.print(erx.getFrameData()[i], HEX);
        Serial.print(F(" "));
      }
      Serial.println();

      if (new_state == 0x00)
      {
        Serial.println(F("Off"));
        SetAttr(erx.getDstEndpoint(), erx.getClusterId(), CURRENT_STATE, new_state, erx.getFrameData()[erx.getDataOffset() + 1]);
      }
      else if (new_state == 0x01)
      {
        Serial.println(F("On"));
        SetAttr(erx.getDstEndpoint(), erx.getClusterId(), CURRENT_STATE, new_state, erx.getFrameData()[erx.getDataOffset() + 1]);
      }
      else
      {
        Serial.print(F("Inv State: "));
        Serial.println(new_state, HEX);
      }
    }
  }
  else if (erx.getDstEndpoint() == TEMP_ENDPOINT)
  {
    Serial.println(F("Temp Ep"));
  }
  else
  {
    Serial.println(F("Inv Ep"));
  }
}
