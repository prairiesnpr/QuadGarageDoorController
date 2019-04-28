#include <SoftwareSerial.h>
#include <XBee.h>
#include <timer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>

#define TEMP_BUS 5
#define XBEE_RST 4

static uint8_t DOOR_OUT_PINS[] =  {12, 13, A0, A1};
static uint8_t DOOR_IN_PINS[] =  {9, 8, 7, 6};

auto timer = timer_create_default(); // create a timer with default settings

//One wire temp sensors
OneWire oneWire(TEMP_BUS);
DallasTemperature sensors(&oneWire);
//Input Temp
DeviceAddress devThermometer = {0x28, 0x2E, 0x53, 0x45, 0x92, 0x17, 0x02, 0x58};

#define UKN_NET_ADDR 0xFFFE
#define HA_PROFILE_ID 0x0104

#define MATCH_DESC_RQST 0x0006
#define MATCH_DESC_RSP 0x8006
#define SIMPLE_DESC_RSP 0x8004
#define ACTIVE_EP_RSP 0x8005
#define ACTIVE_EP_RQST 0x0005
#define SIMPLE_DESC_RQST 0x0004
#define READ_ATTRIBUTES 0x0000

//Input
#define BASIC_CLUSTER_ID 0x0000
#define IDENTIFY_CLUSTER_ID 0x0003
#define GROUPS_CLUSTER_ID 0x0004
#define SCENES_CLUSTER_ID 0x0005
#define ON_OFF_CLUSTER_ID 0x0006
#define LEVEL_CONTROL_CLUSTER_ID 0x0008
#define LIGHT_LINK_CLUSTER_ID 0x1000
#define TEMP_CLUSTER_ID 0x0402
#define HUMIDITY_CLUSTER_ID 0x405
#define BINARY_INPUT_CLUSTER_ID 0x000f
#define IAS_ZONE_CLUSTER_ID 0x0500
#define METERING_CLUSTER_ID 0x0702

//Attr id
#define INSTANTANEOUS_DEMAND 0x0400

//Output
#define OTA_CLUSTER_ID 0x0019 //Upgrade

//Data Type
#define ZCL_CHAR_STR 0x42
#define ZCL_UINT8_T 0x20
#define ZCL_UINT16_T 0x21
#define ZCL_BOOL 0x10

//Device
#define ON_OFF_LIGHT 0x0100
#define DIMMABLE_LIGHT 0x0101
#define TEMPERATURE_SENSOR 0x0302
#define ON_OFF_OUTPUT 0x0002
#define IAS_ZONE 0x0402


uint8_t t_payload[25] = {};

class attribute
{
  public:
    uint16_t id;
    uint8_t* value;
    uint8_t val_len;
    uint8_t type;
    attribute(uint16_t a_id, uint8_t* a_value, uint8_t a_val_len, uint8_t a_type)
    {
      id = a_id;
      value =  new uint8_t[a_val_len];
      memcpy(value, a_value, a_val_len);
      //value = a_value;
      val_len = a_val_len;
      type = a_type;
    }
};



class Cluster
{
  private:
    attribute* attributes;
    uint8_t num_attr;

  public:
    uint16_t id;
    Cluster(uint16_t cl_id, attribute* attr, uint8_t num)
    {
      id = cl_id;
      attributes = attr;
      num_attr = num;
    }
    
    attribute* GetAttr(uint16_t attr_id)
    {
      for (uint8_t i = 0; i < num_attr; i++)
      {
        if (attributes[i].id == attr_id) {
          return &attributes[i];
        }
      }
      Serial.print(F("Attr Not Found: "));
      Serial.println(attr_id, HEX);
      return &attribute{0, 0, 0, 0};
    }

};



class LocalMac
{
  private:
    uint8_t addr;
  public:
    void Set(XBeeAddress64 mac) {
      EEPROM.put(addr, mac);
    }

    XBeeAddress64 Get()
    {
      XBeeAddress64 mac;
      EEPROM.get(addr, mac);
      return mac;
    }
    LocalMac(uint8_t mem_loc = 0) {
      addr = mem_loc;
    }
};


class Endpoint
{
  private:
    uint8_t num_in_clusters;
    uint8_t num_out_clusters;

    Cluster* out_clusters;
    uint16_t dev_type;
  public:
    uint8_t id;
    Cluster* in_clusters;
  public:
    Endpoint(uint8_t ep_id = 0, uint16_t type_dev = 0, Cluster* in_cls = {}, Cluster* out_cls = {}, uint8_t num_in_cls = 0, uint8_t num_out_cls = 0)
    {
      id = ep_id;
      dev_type = type_dev;
      num_in_clusters = num_in_cls;
      num_out_clusters = num_out_cls;
      in_clusters = in_cls;
      out_clusters = out_cls;

    }
    Cluster GetCluster(uint16_t cl_id)
    {
      for (uint8_t i = 0; i < num_in_clusters; i++)
      {
        if (cl_id == in_clusters[i].id) {
          return in_clusters[i];
        }
      }
      Serial.print(F("No Cl "));
      Serial.println(cl_id);
    }
    void GetInClusters(uint16_t* in_cl)
    {
      for (uint8_t i = 0; i < num_in_clusters; i++)
      {
        *(in_cl + i) = in_clusters[i].id;
      }
    }
    void GetOutClusters(uint16_t* out_cl)
    {
      for (uint8_t i = 0; i < num_out_clusters; i++)
      {
        *(out_cl + i) = out_clusters[i].id;
      }
    }
    uint8_t GetNumInClusters() {
      return num_in_clusters;
    }
    uint8_t GetNumOutClusters() {
      return num_out_clusters;
    }
    uint16_t GetDevType() {
      return dev_type;
    }
};

LocalMac macAddr = LocalMac(0);

#define NUM_ENDPOINTS 5

static uint8_t* manuf = (uint8_t*)"iSilentLLC";
static attribute temp_basic_attr[] {{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t*)"Temp", 4, ZCL_CHAR_STR}};
static attribute door1_basic_attr[] {{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t*)"Door 1", 6, ZCL_CHAR_STR}};
static attribute door2_basic_attr[] {{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t*)"Door 2", 6, ZCL_CHAR_STR}};
static attribute door3_basic_attr[] {{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t*)"Door 3", 6, ZCL_CHAR_STR}};
static attribute door4_basic_attr[] {{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t*)"Door 4", 6, ZCL_CHAR_STR}};
static attribute door1_attr[] {{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door2_attr[] {{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door3_attr[] {{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door4_attr[] {{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute temp_attr[] = {{0x0000, 0x00, 1, ZCL_UINT16_T}};

static Cluster door1_in_clusters[] = {Cluster(BASIC_CLUSTER_ID, door1_basic_attr, 2), Cluster(ON_OFF_CLUSTER_ID, door1_attr, 1)};
static Cluster door2_in_clusters[] = {Cluster(BASIC_CLUSTER_ID, door2_basic_attr, 2), Cluster(ON_OFF_CLUSTER_ID, door2_attr, 1)};
static Cluster door3_in_clusters[] = {Cluster(BASIC_CLUSTER_ID, door3_basic_attr, 2), Cluster(ON_OFF_CLUSTER_ID, door3_attr, 1)};
static Cluster door4_in_clusters[] = {Cluster(BASIC_CLUSTER_ID, door4_basic_attr, 2), Cluster(ON_OFF_CLUSTER_ID, door4_attr, 1)};
static Cluster t_in_clusters[] = {Cluster(BASIC_CLUSTER_ID, temp_basic_attr, 2), Cluster(TEMP_CLUSTER_ID, temp_attr, 1)};

static Cluster out_clusters[] = {};
static Endpoint ENDPOINTS[NUM_ENDPOINTS] = {
  Endpoint(1, ON_OFF_OUTPUT, door1_in_clusters, out_clusters, 2, 0),
  Endpoint(2, ON_OFF_OUTPUT, door2_in_clusters, out_clusters, 2, 0),
  Endpoint(3, ON_OFF_OUTPUT, door3_in_clusters, out_clusters, 2, 0),
  Endpoint(4, ON_OFF_OUTPUT, door4_in_clusters, out_clusters, 2, 0),
  Endpoint(5, TEMPERATURE_SENSOR, t_in_clusters, out_clusters, 2, 0),
};

XBeeAddress64 COORDINATOR64 = XBeeAddress64(0, 0);
uint16_t COORDINATOR_NWK = 0x0000;

bool is_joined = 0;
bool start = 0;
uint8_t associated = 1;
bool setup_complete = 0;
bool nwk_pending = 0;
bool assc_pending = 0;

uint8_t cmd_frame_id;
typedef void (*cmd_ptr)();
cmd_ptr last_command;
uint8_t cmd_seq_id;


#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))
uint64_t SWAP_UINT64(uint64_t num)
{
  uint64_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7;
  byte0 = (num & 0x00000000000000FF) >> 0 ;
  byte1 = (num & 0x000000000000FF00) >> 8 ;
  byte2 = (num & 0x0000000000FF0000) >> 16 ;
  byte3 = (num & 0x00000000FF000000) >> 24 ;
  byte4 = (num & 0x000000FF00000000) >> 32 ;
  byte5 = (num & 0x0000FF0000000000) >> 40 ;
  byte6 = (num & 0x00FF000000000000) >> 48 ;
  byte7 = (num & 0xFF00000000000000) >> 56 ;
  return ((byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | (byte7 << 0));
}

Endpoint GetEndpoint(uint8_t ep_id)
{
  for (uint8_t i = 0; i < NUM_ENDPOINTS; i++) {
    if (ENDPOINTS[i].id == ep_id) {
      return ENDPOINTS[i];
    }
  }
}




// Define SoftSerial TX/RX pins
// Connect Arduino pin 10 to TX of usb-serial device
#define ssRX 10
// Connect Arduino pin 11 to RX of usb-serial device
#define ssTX 11

//XBeeAddress64 macAddr;


uint8_t netAddr[2];
uint8_t seqID;

SoftwareSerial nss(ssRX, ssTX);
XBeeWithCallbacks  xbee;

// ieee high
static const uint8_t shCmd[] = {'S', 'H'};
// ieee low
static const uint8_t slCmd[] = {'S', 'L'};
// association status
static const uint8_t assocCmd[] = {'A', 'I'};
// panID
static const uint8_t netCmd[] = {'M', 'Y'};

ZBExplicitTxRequest exp_tx = ZBExplicitTxRequest();

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

  sensors.setResolution(devThermometer, 9);

  delay(5000);

  //Set up callbacks
  xbee.onZBExplicitRxResponse(zdoReceive);
  xbee.onZBTxStatusResponse(zbTxStatusResp);
  xbee.onAtCommandResponse(atCmdResp);
  xbee.onOtherResponse(otherResp);

  getMAC();
  Serial.print(F("LCL Add: "));
  printAddr(macAddr.Get());

  timer.every(30000, update_temp); 
}


//Update temp, serial for now
bool  update_temp(void *) {
  uint8_t t_ep_id = 5;
  
  sensors.requestTemperatures();

  float TempC = sensors.getTempC(devThermometer);

  Serial.print(F("Dev Temp: "));
  Serial.print(TempC);
  Serial.println(F("°C"));

  if (TempC < 200 && TempC > -100){
    //Seeing some 900 deg readings, just filtering them out
    Endpoint end_point = GetEndpoint(t_ep_id);
    Cluster cluster = end_point.GetCluster(TEMP_CLUSTER_ID);
    attribute* attr = cluster.GetAttr(0x0000);
    attr->val_len = 2;
    uint16_t cor_t = (uint16_t)(TempC * 100.0);
  
    uint8_t t_value[2] = {(uint8_t)cor_t,
                          (uint8_t)(cor_t >> 8)
                         };
    attr->value = t_value;
    sendAttributeRpt(cluster.id, attr, end_point.id, end_point.id);
  }
  return true;
}

void printAddr(uint64_t val) {
  uint32_t msb = val >> 32;
  uint32_t lsb = val;
  Serial.print(msb, HEX);
  Serial.println(lsb, HEX);
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
      sendAttributeWriteRsp(cluster_id, attr, ep_id, ep_id, value); 
      delay(2000);
      digitalWrite(DOOR_OUT_PINS[end_point.id - 1], LOW);
      //This is a bit strange, but we are using inverted logic, it's open 
      sendAttributeWriteRsp(cluster_id, attr, ep_id, ep_id, *attr->value);     
    }
  }
}

void loop() {
  xbee.loop();
  
  if (associated != 0 && !assc_pending) {
    assc_pending = 1;
    getAssociation();
  }
  if (netAddr[0] == 0 && netAddr[1] == 0 && !nwk_pending && !assc_pending) {
    nwk_pending = 1;
    getNetAddr();
  }
  if (!nwk_pending && !assc_pending) {
    //Serial.println("Config Cmp");
    setup_complete = 1;
  }
  if (setup_complete && !start) {
    sendDevAnnounce();

    start = 1;
  }
  if (start) {
    
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
        Serial.println(val);
        //uint8_t door_state = *attr->value ^ 1;
        *attr->value = val;
        sendAttributeRpt(cluster.id, attr, end_point.id, end_point.id);
      }
      
    }
  }


  timer.tick();
  wdt_reset();
}


void zbTxStatusResp(ZBTxStatusResponse& resp, uintptr_t) {
  if (resp.isSuccess()) {
    Serial.println(F("TX OK"));

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
  Serial.println(F("Other Response"));
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

void sendAttributeWriteRsp(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep,  uint8_t result) {
  /*
    payload
    byte 0: frame control
    byte 1 Seq
    byte 2 cmd id
    byte 3-4: Attr Id
    byte 5: type
    bytes6: Success 0x01
    -----------------------------
    CMDS: 0x0A Report Attr
          0x01 Read Attr Response
          0x0D Discover Attributes Response
          0x04 Write Attr Response

  */
  uint8_t payload_len = 6 + attr->val_len;

  uint8_t pre[payload_len] = {0x00,
                              cmd_seq_id,
                              0x0A,  //0x04, //Write attr resp
                              static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                              static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                              attr->type,
                              result
                             };


  memcpy(t_payload, pre, sizeof(pre));
  cmd_frame_id = xbee.getNextFrameId();

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    Serial.println(F("Sent Attr Write Rsp"));

  }
}

void sendAttributeRpt(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep) {
  /*
    payload
    byte 0: frame control
    byte 1 Seq
    byte 2 cmd id
    byte 3-4: Attr Id
    byte 5: type
    bytes[] value in little endian
    -----------------------------
    CMDS: 0x0A Report Attr
          0x01 Read Attr Response
          0x0D Discover Attributes Response
          0x04 Write Attr Response

  */


  uint8_t payload_len;
  if (attr->type == ZCL_CHAR_STR){
    payload_len = 7 + attr->val_len;
    uint8_t pre[] = {0x00, 
                     cmd_seq_id,
                     0x0A, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     attr->type,
                     attr->val_len,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 7, attr->value, attr->val_len);
  }
  else {
    payload_len = 6 + attr->val_len;
    uint8_t pre[] = {0x00,
                     cmd_seq_id,
                     0x0A, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     attr->type,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 6, attr->value, attr->val_len);
  }
  
  cmd_frame_id = xbee.getNextFrameId();

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    Serial.println(F("Sent Attr Rpt"));

  }
}


void sendAttributeRsp(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep, uint8_t cmd) {
  /*
    payload
    byte 0: frame control
    byte 1 Seq
    byte 2 cmd id
    byte 3-4: Attr Id
    byte 5: type
    bytes[] value in little endian
    -----------------------------
    CMDS: 0x0A Report Attr
          0x01 Read Attr Response
          0x0D Discover Attributes Response
          0x04 Write Attr Response

  */

  uint8_t payload_len;
  if (attr->type == ZCL_CHAR_STR){
    payload_len = 8 + attr->val_len;
    uint8_t pre[] = {0x00, 
                     cmd_seq_id,
                     cmd, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     0x00,//status
                     attr->type,
                     attr->val_len,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 8, attr->value, attr->val_len);
  }
  else {
    payload_len = 7 + attr->val_len;
    uint8_t pre[] = {0x00,
                     cmd_seq_id,
                     cmd, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     0x00,
                     attr->type,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 7, attr->value, attr->val_len);
  }
  
  // }



  cmd_frame_id = xbee.getNextFrameId();
  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    Serial.println(F("Sent Attr Rsp"));

  }
}

void sendDevAnnounce() {
  /*
     12 byte data payload
     byte 0: Sequence Number
     byte 1-2: Net Addr in Little Endian
     byte 3-10: Mac Addr in Little Endian
     byte 11: Mac Capability Flag, 0x8C = Mains powered device; receiver on when idle; address not self-assigned.
  */
  uint64_t mac = SWAP_UINT64(macAddr.Get());
  uint8_t payload[] = {seqID++,
                       netAddr[1],
                       netAddr[0],
                       static_cast<uint8_t>((mac & 0xFF00000000000000) >> 56),
                       static_cast<uint8_t>((mac & 0x00FF000000000000) >> 48),
                       static_cast<uint8_t>((mac & 0x0000FF0000000000) >> 40),
                       static_cast<uint8_t>((mac & 0x000000FF00000000) >> 32),
                       static_cast<uint8_t>((mac & 0x00000000FF000000) >> 24),
                       static_cast<uint8_t>((mac & 0x0000000000FF0000) >> 16),
                       static_cast<uint8_t>((mac & 0x000000000000FF00) >> 8),
                       static_cast<uint8_t>((mac & 0x00000000000000FF) >> 0),
                       0x8C
                      };

  cmd_frame_id = xbee.getNextFrameId();
  last_command = &sendDevAnnounce;

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               0x00FD,
                               0x00,    //broadcast radius
                               0x00,    //option
                               payload, //payload
                               sizeof(payload),    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               0x0013,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);
  Serial.println(F("Send Dev Ann"));
}

void build_payload_list(const uint16_t *values, const uint8_t v_size, uint8_t* res) {
  //Build byte Payload in little endian order
  *res = v_size / 2;
  uint8_t c = 0;
  for (uint8_t i = 1; i < (2 * v_size + 1); i += 2) {
    *(res + i) = static_cast<uint8_t>((values[c] & 0x00FF) >> 0);
    *(res + i + 1) = static_cast<uint8_t>((values[c] & 0xFF00) >> 8);
    c++;
  }
}


void print_payload(uint8_t* payload, uint8_t len) {
  Serial.print(F("PLD: "));
  for (uint8_t i = 0; i < len; i++) {
    if (i == 0) {
      Serial.print(*payload, HEX);
    }
    else {
      Serial.print(*(payload + i), HEX);
    }
  }
  Serial.println();
}

void sendActiveEpResp() {
  /*
     byte 0 sequence number
     byte 1 status 00 success
     byte 2-3 NWK little endian
     byte 4 Number of active endpoints
     List of active endpoints
  */
  cmd_frame_id = xbee.getNextFrameId();
  last_command = &sendActiveEpResp;
  uint8_t len_payload = 5 + NUM_ENDPOINTS;
  uint8_t payload[len_payload] = {cmd_seq_id, //Has to match requesting packet
                                  0x00,
                                  netAddr[1],
                                  netAddr[0],
                                  NUM_ENDPOINTS,
                                 };

  uint8_t i = 5;
  uint8_t cl_i = 0;
  for (i; i < len_payload; i++) {
    payload[i] = ENDPOINTS[cl_i].id;
    cl_i++;
  }

  Serial.print(F("Actv Ep Rsp Pld: "));
  for (int j = 0; j < len_payload; j++) {
    Serial.print(payload[j], HEX);
    Serial.print(" ");
  }
  Serial.println("");


  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               UKN_NET_ADDR,
                               0x00,    //broadcast radius
                               0x00,    //option
                               payload, //payload
                               len_payload,    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               ACTIVE_EP_RSP,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);

  Serial.println(F("Send Actv Ep Resp"));



}

void sendSimpleDescRpt(uint8_t ep) {
  /*
     byte 0: Sequence number, match requesting packet
     byte 1: status 00= Success
     byte 2-3: NWK in little endian
     byte 4: Length of Simple Descriptor Report
     byte 5: End point report, which endpoint is being reported
     byte 6-7: profile id in little endian 0x0104
     byte 7-8: Device Type in little endian, 0x0007 is combined interface see page 51 of Zigbee HA profile
     byte 9: version number (App Dev)
     byte 10: Input Cluster Count
     byte [] List of output clusters in little endian format
     byte n+1: Output Cluster Count
     byte [] List of output clusters in little endian format
  */



  cmd_frame_id = xbee.getNextFrameId();
  uint8_t num_out = ENDPOINTS[(ep - 1)].GetNumOutClusters();  
  uint8_t out_len = 0;
  if (num_out > 0)
  {
     out_len = 2 *num_out + 1;
  }
  uint8_t num_in = ENDPOINTS[(ep - 1)].GetNumInClusters();  
  uint8_t in_len = 0;
  if (num_in > 0)
  {
     in_len = 2 * num_in + 1;
  }
  uint8_t pre_len = 11;
  uint8_t payload_len = pre_len + out_len + in_len;

  uint8_t pre[] = {cmd_seq_id,
                   0x00,
                   netAddr[1],
                   netAddr[0],
                   static_cast<uint8_t>(out_len + in_len + 6), //Length of simple descriptor
                   ep,
                   static_cast<uint8_t>((HA_PROFILE_ID & 0x00FF) >> 0),
                   static_cast<uint8_t>((HA_PROFILE_ID & 0xFF00) >> 8),
                   static_cast<uint8_t>((ENDPOINTS[(ep - 1)].GetDevType() & 0x00FF) >> 0), //Fix me
                   static_cast<uint8_t>((ENDPOINTS[(ep - 1)].GetDevType() & 0xFF00) >> 8),
                   0x01, //Don't Care (App Version)
                  };

  uint8_t in_clusters[in_len];

  memcpy(t_payload, pre, pre_len);

  uint16_t in_cl[num_in];
  ENDPOINTS[(ep - 1)].GetInClusters(in_cl);
  build_payload_list(in_cl, 2 * num_in, in_clusters);
 
  memcpy(t_payload + pre_len , in_clusters, sizeof(in_clusters));

  uint8_t out_clusters[out_len];
  uint16_t out_cl[num_out];
  ENDPOINTS[(ep - 1)].GetOutClusters(out_cl);
  build_payload_list(out_cl, 2 * num_out, out_clusters);
 
  memcpy(t_payload + pre_len + sizeof(in_clusters) , out_clusters, sizeof(out_clusters));

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               UKN_NET_ADDR,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               SIMPLE_DESC_RSP,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);

  Serial.print(F("Send Smpl Desc Rpt: "));
  Serial.println(ep, HEX);
}

void sendBasicClusterResp() {
}

void getNetAddr() {
  AtCommandRequest atRequest = AtCommandRequest();
  atRequest.setCommand((uint8_t*)netCmd); //breaking(uint8_t*)at
  xbee.send(atRequest);
}

uint32_t packArray(uint8_t *val) {
  uint32_t res = 0;
  for (int i = 0; i < 4; i++) {
    res = res << 8 | val[i];
  }
  return res;
}

void getAssociation() {
  AtCommandRequest atRequest = AtCommandRequest();
  atRequest.setCommand((uint8_t*)assocCmd); //breaking
  xbee.send(atRequest);
}

void getMAC() {
  uint8_t msb[4];
  uint8_t lsb[4];
  AtCommandResponse atResponse = AtCommandResponse();
  AtCommandRequest atRequest = AtCommandRequest();
  bool success = 0;
  while (success == 0) {
    atRequest.setCommand((uint8_t*)shCmd); //breaking
    xbee.send(atRequest);
    success = waitforResponse(msb);
  }
  success = 0;
  while (success == 0) {
    atRequest.setCommand((uint8_t*)slCmd); //breaking
    xbee.send(atRequest);
    success = waitforResponse(lsb);
  }
  macAddr.Set(XBeeAddress64(packArray(msb), packArray(lsb)));
}

bool waitforResponse(uint8_t *val) {
  AtCommandResponse atResponse = AtCommandResponse();
  if (xbee.readPacket(5000)) {
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
      xbee.getResponse().getAtCommandResponse(atResponse);
      if (atResponse.isOk()) {
        if (atResponse.getValueLength() > 0) {
          //Serial.print("Size of: ");
          //Serial.println(atResponse.getValueLength());
          for (int i = 0; i < atResponse.getValueLength(); i++) {
            val[i] = atResponse.getValue()[i];
          }
          return 1;
        }
      }
      else {
        Serial.print(F("Cmd rtrn err: "));
        Serial.println(atResponse.getStatus(), HEX);
      }
    } else {
      Serial.print(F("Exp AT got "));
      Serial.print(xbee.getResponse().getApiId(), HEX);
    }
  } else {
    // at command failed
    if (xbee.getResponse().isError()) {
      Serial.print(F("Er rd pkt. EC: "));
      Serial.println(xbee.getResponse().getErrorCode());
    }
    else {
      Serial.print(F("No rsp"));
    }
  }
  return 0;
}
