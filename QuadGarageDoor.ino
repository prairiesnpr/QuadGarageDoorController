#include <SoftwareSerial.h>
#include <arduino-timer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>
#include <zha_constants.h>
#include "zha/device_details.h"
#include <zha_functions.h>


#define TEMP_BUS 5
#define XBEE_RST 4

// Define SoftSerial TX/RX pins
// Connect Arduino pin 10 to TX of usb-serial device
#define ssRX 10
// Connect Arduino pin 11 to RX of usb-serial device
#define ssTX 11

#define TEMP_ENDPOINT 5

static uint8_t DOOR_OUT_PINS[] =  {12, 13, A0, A1};
static uint8_t DOOR_IN_PINS[] =  {9, 8, 7, 6};

void(* resetFunc) (void) = 0;

auto timer = timer_create_default(); // create a timer with default settings

unsigned long loop_time = millis();
unsigned long last_msg_time = loop_time - 1000;

//One wire temp sensors
OneWire oneWire(TEMP_BUS);
DallasTemperature sensors(&oneWire);
//Input Temp
DeviceAddress devThermometer = {0x28, 0x2E, 0x53, 0x45, 0x92, 0x17, 0x02, 0x58};

bool is_joined = 0;
bool start = 0;
uint8_t associated = 1;
bool setup_complete = 0;
bool nwk_pending = 0;
bool assc_pending = 0;
bool *cmd_result;
bool awt_announce = 0;

uint8_t t_value[2] = {0x00, 0x00}; 

SoftwareSerial nss(ssRX, ssTX);

void setup() {
  wdt_enable(WDTO_8S);
  pinMode(XBEE_RST, OUTPUT);
  
  //Reset xbee
  digitalWrite(XBEE_RST, LOW);
  digitalWrite(XBEE_RST, HIGH);
  
  for (int i = 0; i < 4; i++) {
    pinMode(DOOR_OUT_PINS[i], OUTPUT);
    digitalWrite(DOOR_OUT_PINS[i], LOW);
    pinMode(DOOR_IN_PINS[i], INPUT);
  }
  Serial.begin(9600);
  Serial.println(F("Startup"));
  sensors.begin();
  nss.begin(9600);
  xbee.setSerial(nss);

  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(F(" Temp Dev."));

  bool success = sensors.getAddress(devThermometer, 0);

  if (success)
  { 
    Serial.print(F("Dev Addr: "));
    for(int i;i<sizeof(devThermometer);i++)
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
  Serial.println(F("Res Set, Dly St"));
  delay(5000);
  Serial.println(F("Dly Cmp"));

  //Set up callbacks
  xbee.onZBExplicitRxResponse(zdoReceive);
  xbee.onZBTxStatusResponse(zbTxStatusResp);
  xbee.onAtCommandResponse(atCmdResp);
  xbee.onOtherResponse(otherResp);

  getMAC();
  Serial.print(F("LCL Add: "));
  printAddr(macAddr.Get());

  timer.every(30000, update_sensors); 
}

bool read_temp(DeviceAddress thermometer, uint8_t *t_value)
{
  bool success = sensors.requestTemperaturesByAddress(thermometer);

  if (!success)
  {
    Serial.println(F("T addr not found"));
  }

  delay(10); //No idea why this is needed

  float TempC = sensors.getTempC(thermometer);
  if (TempC == DEVICE_DISCONNECTED_C)
  {
    Serial.println(F("T Disconnect"));
    return false;
  } 

  Serial.print(F("Temp: "));
  Serial.print(String(TempC, 2));
  Serial.println(F("°C"));

  uint16_t cor_t = (uint16_t)(TempC * 100.0);
  t_value[0] = (uint8_t)cor_t;
  t_value[1] = (uint8_t)(cor_t >> 8);
  return true;
}

void update_temp()
{
  //Temp
  bool r_success = read_temp(devThermometer, t_value);

  if (r_success)
  { 
    Endpoint end_point = GetEndpoint(TEMP_ENDPOINT);
    Cluster cluster = end_point.GetCluster(TEMP_CLUSTER_ID);
    attribute* attr = cluster.GetAttr(0x0000);
    attr->value = t_value;
    sendAttributeRpt(cluster.id, attr, end_point.id, 1);
  }
}

bool update_sensors(void *) {
  update_temp();
  return true; 
}

void SetAttr(uint8_t ep_id, uint16_t cluster_id, uint16_t attr_id, uint8_t value)
{
  Endpoint end_point = GetEndpoint(ep_id);
  Cluster cluster = end_point.GetCluster(cluster_id);
  attribute* attr = cluster.GetAttr(attr_id);
  Serial.println(cluster_id);
  if (cluster_id == ON_OFF_CLUSTER_ID) {
    //*attr->value = value; //breaking
    //We don't want to set value here, value is set by the door opening or closing
    if (value == 0x00 || 0x01) {
      Serial.print(F("Toggle: "));
      Serial.println(end_point.id);
      digitalWrite(DOOR_OUT_PINS[end_point.id - 1], HIGH);
      sendAttributeWriteRsp(cluster_id, attr, ep_id, 1, value); //Tell sender that we did what we were told to
      delay(2000);
      digitalWrite(DOOR_OUT_PINS[end_point.id - 1], LOW);
      //This is a bit strange, but we are using inverted logic, it's open 
      //attr->value = 0x00;
      //sendAttributeRpt(cluster_id, attr, ep_id, 1);
      //sendAttributeWriteRsp(cluster_id, attr, ep_id, 1, *attr->value);     
    }
  }
}

uint8_t start_fails = 0;

void loop() {
  //Serial.println(F("Loop"));
  xbee.loop();
  
  if (!associated && !assc_pending) {
    assc_pending = 1;
    Serial.println(F("Pending Assc"));
    getAssociation();
  }
  if (associated && !assc_pending && !setup_complete)
  {
    Serial.println(F("Assc"));
    assc_pending = 0;
  }
  if (netAddr[0] == 0 && netAddr[1] == 0 && !nwk_pending && !assc_pending) {
    nwk_pending = 1;
    Serial.println(F("Pending NWK"));
    getNetAddr();
  }
  if (netAddr[0] != 0 && netAddr[1] != 0 && nwk_pending)
  {
    nwk_pending = 0;
    Serial.println(F("NWK"));
  }
  if (!setup_complete && !nwk_pending && !assc_pending) {
    Serial.println(F("Config Cmp"));
    setup_complete = 1;
  }
  if (setup_complete && !start && !awt_announce) {
    //start = 1;
    awt_announce = 1;
    Serial.println(F("Dev Annc"));
    cmd_result = &start;
    sendDevAnnounce();
  }
  if (start) {
    //Serial.println(F("St Scan Lp")); 
    for (int i = 0; i < 4; i++) {
      uint8_t val = digitalRead(DOOR_IN_PINS[i]) ^ 1;
      Endpoint end_point = GetEndpoint(i + 1);
      Cluster cluster = end_point.GetCluster(ON_OFF_CLUSTER_ID);
      attribute* attr = cluster.GetAttr(0x0000);

      if (val != *attr->value)
      {
        Serial.print(F("EP"));
        Serial.print(end_point.id);
        Serial.print(F(": "));
        Serial.print(*attr->value);
        Serial.print(F(" Now "));
        *attr->value = val;
        Serial.println(*attr->value);
        sendAttributeRpt(cluster.id, attr, end_point.id, 1);
      }
      
    }
  }
  else if ((loop_time - last_msg_time) > 1000)
  {
    Serial.println(F("Not Started.."));
    last_msg_time = millis();
    if (start_fails > 20)
    {
      resetFunc();
    }
    start_fails++;
  }


  timer.tick();
  wdt_reset();
  loop_time = millis();

}


void zbTxStatusResp(ZBTxStatusResponse& resp, uintptr_t) {
  if (resp.isSuccess()) {
    Serial.println(F("TX OK"));
    *cmd_result = 1; 
  }
  else {
    Serial.println(F("TX FAIL"));
    Serial.println(resp.getDeliveryStatus(), HEX);

    if (resp.getFrameId() == cmd_frame_id) {
      last_command();
    }
  }
}



void otherResp(XBeeResponse& resp, uintptr_t) {
  Serial.println(F("Other Response: "));
}

void atCmdResp(AtCommandResponse& resp, uintptr_t) {
  Serial.println(F("At resp"));
  if (resp.getStatus() == AT_OK) {
    if (resp.getCommand()[0] == assocCmd[0] &&
        resp.getCommand()[1] == assocCmd[1]) {
      //Association Status
      associated = resp.getValue()[0];
      assc_pending = 0;
      Serial.print(F("Asc St: "));
      Serial.println(associated);
    }
    else if (resp.getCommand()[0] == netCmd[0] &&
             resp.getCommand()[1] == netCmd[1]) {
      //NWK
      for (int i = 0; i < resp.getValueLength(); i++) {
        Serial.print(resp.getValue()[i], HEX);
        Serial.print(F(" "));
      }
      Serial.println();
      netAddr[0] = resp.getValue()[0];
      netAddr[1] = resp.getValue()[1];
      nwk_pending = 0;
      Serial.print(F("NWK: "));
      Serial.print(netAddr[0], HEX);
      Serial.println(netAddr[1], HEX);
    }
    else {
      Serial.println(F("Ukn Cmd"));
    }
  }
  else {
    Serial.println(F("AT Fail"));
  }
}

void zdoReceive(ZBExplicitRxResponse& erx, uintptr_t) {
  // Create a reply packet containing the same data
  // This directly reuses the rx data array, which is ok since the tx
  // packet is sent before any new response is received
  
  if (erx.getRemoteAddress16() == 0 ) {
    Serial.println(F("ZDO"));
    Serial.println(erx.getClusterId(), HEX);
    if (erx.getClusterId() == ACTIVE_EP_RQST) {
      //Have to match sequence number in response
      cmd_seq_id = erx.getFrameData()[erx.getDataOffset()];
      cmd_result = NULL;
      sendActiveEpResp();
    }
    else if (erx.getClusterId() == SIMPLE_DESC_RQST) {
      Serial.print("Actv Ep Rqst: ");
      //Have to match sequence number in response
      cmd_seq_id = erx.getFrameData()[erx.getDataOffset()];
      //Payload is EndPoint
      uint8_t ep = erx.getFrameData()[erx.getDataOffset() + 3];
      Serial.println(ep, HEX);
      sendSimpleDescRpt(ep);
    }
    else if (erx.getClusterId() == ON_OFF_CLUSTER_ID) {
      Serial.println(F("ON/OFF Cl"));
      uint8_t len_data = erx.getDataLength() - 3;
      uint16_t attr_rqst[len_data / 2];
      for (uint8_t i = erx.getDataOffset(); i < (erx.getDataLength() + erx.getDataOffset() + 3); i ++) {
        Serial.print(erx.getFrameData()[i]);
      }
      Serial.println();
      cmd_seq_id = erx.getFrameData()[erx.getDataOffset() + 1];
      uint8_t ep = erx.getDstEndpoint();
      uint8_t cmd_id = erx.getFrameData()[erx.getDataOffset() + 2];
      Endpoint end_point = GetEndpoint(ep);
      if (cmd_id == 0x00) {
        Serial.println(F("Cmd Off"));
        SetAttr(ep, erx.getClusterId(), 0x0000, 0x00);
      }
      else if (cmd_id == 0x01) {
        Serial.println(F("Cmd On"));
        SetAttr(ep, erx.getClusterId(), 0x0000, 0x01);
      }
      else {
        Serial.print(F("Cmd Id: "));
        Serial.println(cmd_id, HEX);
      }
    }
    else if (erx.getClusterId() == READ_ATTRIBUTES) { //SHould be basic cluster id
      Serial.println(F("Clstr Rd Att:"));
      cmd_seq_id = erx.getFrameData()[erx.getDataOffset() + 1];
      uint8_t ep = erx.getDstEndpoint();
      //cmd_seq_id = erx.getFrameData()[erx.getDataOffset()];
      Serial.print(F("Cmd Seq: "));
      Serial.println(cmd_seq_id);

      uint8_t len_data = erx.getDataLength() - 3;
      uint16_t attr_rqst[len_data / 2];

      Endpoint end_point = GetEndpoint(ep);
      for (uint8_t i = erx.getDataOffset() + 3; i < (len_data + erx.getDataOffset() + 3); i += 2) {
        attr_rqst[i / 2] = (erx.getFrameData()[i + 1] << 8) |
                           (erx.getFrameData()[i] & 0xff);
        attribute* attr = end_point.GetCluster(erx.getClusterId()).GetAttr(attr_rqst[i / 2]);
        sendAttributeRsp(erx.getClusterId(), attr, ep, ep, 0x01);
      }

    }
  }
}

