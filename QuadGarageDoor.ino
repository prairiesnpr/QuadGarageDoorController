#include <SoftwareSerial.h>
#include <arduino-timer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>
#include <xbee_zha.h>
#include "zha/device_details.h"

#define TEMP_BUS 5
#define XBEE_RST 4

// Define SoftSerial TX/RX pins
#define ssRX 10
#define ssTX 11

#define TEMP_ENDPOINT 5

#define START_LOOPS 100

uint8_t start_fails = 0;

static uint8_t DOOR_OUT_PINS[] = {12, 13, A0, A1};
static uint8_t DOOR_IN_PINS[] = {9, 8, 7, 6};

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

void setup()
{
  pinMode(XBEE_RST, OUTPUT);

  // Reset xbee
  digitalWrite(XBEE_RST, LOW);
  pinMode(XBEE_RST, INPUT);

  // Configure Door Pins
  for (int i = 0; i < 4; i++)
  {
    pinMode(DOOR_OUT_PINS[i], OUTPUT);
    digitalWrite(DOOR_OUT_PINS[i], LOW);
    pinMode(DOOR_IN_PINS[i], INPUT);
  }

  Serial.begin(9600);
  Serial.println(F("Startup"));
  sensors.begin();
  nss.begin(9600);

  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(F(" Temp Dev."));

  bool success = sensors.getAddress(devThermometer, 0);

  if (success)
  {
    Serial.print(F("Dev Addr: "));
    for (int i; i < sizeof(devThermometer); i++)
    {
      Serial.print(devThermometer[i]);
      Serial.print(" ");
    }
    Serial.println();
  }
  else
  {
    Serial.println(F("T Dev Addr Fail"));
  }
  sensors.setResolution(devThermometer, 9);

  zha.Start(nss, zdoReceive, NUM_ENDPOINTS, ENDPOINTS);

  // Set up callbacks
  zha.registerCallbacks(atCmdResp, zbTxStatusResp, otherResp);

  Serial.println(F("CB Conf"));

  timer.every(30000, update_sensors);
  wdt_enable(WDTO_8S);
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
    attribute *attr = cluster.GetAttr(0x0000);
    attr->SetValue(res_temp);
    zha.sendAttributeRpt(cluster.id, attr, end_point.id, 1);
  }
}

bool update_sensors(void *)
{
  update_temp();
  return true;
}

void SetAttr(uint8_t ep_id, uint16_t cluster_id, uint16_t attr_id, uint8_t value, uint8_t rqst_seq_id)
{
  Endpoint end_point = zha.GetEndpoint(ep_id);
  Cluster cluster = end_point.GetCluster(cluster_id);
  attribute *attr = cluster.GetAttr(attr_id);

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
      zha.sendAttributeWriteRsp(cluster_id, attr, ep_id, 1, value, zha.cmd_seq_id); // Tell sender that we did what we were told to
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
    for (int i = 0; i < 4; i++)
    {
      uint8_t val = digitalRead(DOOR_IN_PINS[i]) ^ 1;
      Endpoint end_point = zha.GetEndpoint(i + 1);
      Cluster cluster = end_point.GetCluster(ON_OFF_CLUSTER_ID);
      attribute *attr = cluster.GetAttr(0x0000);

      if (val != attr->GetIntValue())
      {
        Serial.print(F("EP"));
        Serial.print(end_point.id);
        Serial.print(F(": "));
        Serial.print(attr->GetIntValue());
        Serial.print(F(" Now "));
        *attr->value = val;
        Serial.println(*attr->value);
        zha.sendAttributeRpt(cluster.id, attr, end_point.id, 1);
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
    if (start_fails > START_LOOPS)
    {
      resetFunc();
    }
    start_fails++;
  }

  timer.tick();
  wdt_reset();
  loop_time = millis();
}

void zdoReceive(ZBExplicitRxResponse &erx, uintptr_t)
{
  // Create a reply packet containing the same data
  // This directly reuses the rx data array, which is ok since the tx
  // packet is sent before any new response is received

  if (erx.getRemoteAddress16() == 0)
  {
    zha.cmd_seq_id = erx.getFrameData()[erx.getDataOffset() + 1];
    Serial.print(F("Cmd Seq: "));
    Serial.println(zha.cmd_seq_id);

    uint8_t ep = erx.getDstEndpoint();
    uint16_t clId = erx.getClusterId();
    uint8_t cmd_id = erx.getFrameData()[erx.getDataOffset() + 2];
    uint8_t frame_type = erx.getFrameData()[erx.getDataOffset()] & 0x03;

    if (frame_type)
    {
      Serial.println(F("Clstr Cmd"));
      if (ep < TEMP_ENDPOINT)
      {
        Serial.println(F("Door Ep"));
        if (clId == ON_OFF_CLUSTER_ID)
        {
          Serial.println(F("ON/OFF Cl"));
          uint8_t len_data = erx.getDataLength() - 3;
          uint16_t attr_rqst[len_data / 2];
          uint8_t new_state = erx.getFrameData()[erx.getDataOffset() + 2];

          for (uint8_t i = erx.getDataOffset(); i < (erx.getDataLength() + erx.getDataOffset() + 3); i++)
          {
            Serial.print(erx.getFrameData()[i], HEX);
            Serial.print(F(" "));
          }
          Serial.println();

          if (cmd_id == 0x00)
          {
            Serial.println(F("Cmd Off"));
            SetAttr(ep, erx.getClusterId(), 0x0000, 0x00, erx.getFrameData()[erx.getDataOffset() + 1]);
          }
          else if (cmd_id == 0x01)
          {
            Serial.println(F("Cmd On"));
            SetAttr(ep, erx.getClusterId(), 0x0000, 0x01, erx.getFrameData()[erx.getDataOffset() + 1]);
          }
          else
          {
            Serial.print(F("Cmd Id: "));
            Serial.println(cmd_id, HEX);
          }
        }
      }
      else if (ep == TEMP_ENDPOINT)
      {
        Serial.println(F("Temp Ep"));
      }
      else
      {
        Serial.println(F("Inv Ep"));
      }
    }
    else
    {
      Serial.println(F("Glbl Cmd"));

      Endpoint end_point = zha.GetEndpoint(ep);
      Cluster cluster = end_point.GetCluster(clId);
      if (cmd_id == 0x00)
      {
        // Read attributes
        Serial.println(F("Read Attr"));
        uint8_t len_data = erx.getDataLength() - 3;
        uint16_t attr_rqst[len_data / 2];
        for (uint8_t i = erx.getDataOffset() + 3; i < (len_data + erx.getDataOffset() + 3); i += 2)
        {
          attr_rqst[i / 2] = (erx.getFrameData()[i + 1] << 8) |
                             (erx.getFrameData()[i] & 0xff);
          attribute *attr = end_point.GetCluster(erx.getClusterId()).GetAttr(attr_rqst[i / 2]);
          Serial.print(F("Clstr Rd Att: "));
          Serial.println(attr_rqst[i / 2]);
          zha.sendAttributeRsp(erx.getClusterId(), attr, ep, 0x01, 0x01, zha.cmd_seq_id);
          zha.cmd_seq_id++;
        }
      }
      else
      {
        Serial.println(F("Not Read Attr"));
      }
    }
    uint8_t frame_direction = (erx.getFrameData()[erx.getDataOffset()] >> 3) & 1;
    if (frame_direction)
    {
      Serial.println(F("Srv to Client"));
    }
    else
    {
      Serial.println(F("Client to Srv"));
    }
    Serial.print(F("ZDO: EP: "));
    Serial.print(ep);
    Serial.print(F(", Clstr: "));
    Serial.print(clId, HEX);
    Serial.print(F(" Cmd Id: "));
    Serial.print(cmd_id, HEX);
    Serial.print(F(" FrmCtl: "));
    Serial.println(erx.getFrameData()[erx.getDataOffset()], BIN);

    if (erx.getClusterId() == ACTIVE_EP_RQST)
    {
      // Have to match sequence number in response
      cmd_result = NULL;
      zha.last_seq_id = erx.getFrameData()[erx.getDataOffset()];
      zha.sendActiveEpResp(zha.last_seq_id);
    }
    if (erx.getClusterId() == SIMPLE_DESC_RQST)
    {
      Serial.print("Simple Desc Rqst, Ep: ");
      // Have to match sequence number in response
      // Payload is EndPoint
      // Can this just be regular ep?
      uint8_t ep_msg = erx.getFrameData()[erx.getDataOffset() + 3];
      Serial.println(ep_msg, HEX);
      zha.sendSimpleDescRpt(ep_msg, erx.getFrameData()[erx.getDataOffset()]);
    }
  }
}
