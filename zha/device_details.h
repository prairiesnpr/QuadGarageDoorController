#include <stdint.h>

#define NUM_ENDPOINTS 6

constexpr uint8_t one_zero_byte[] = {0x00};
constexpr uint8_t one_max_byte[] = {0xFF};
constexpr uint8_t two_zero_byte[] = {0x00, 0x00};
constexpr uint8_t four_zero_byte[] = {0x00, 0x00, 0x00, 0x00};
constexpr uint8_t zcl_version[] = {0x08};
constexpr uint8_t power_source[] = {0x01};

// Reuse these to save SRAM
#define manufacturer "iSilentLLC"
#define model "Quad Door Controller"

attribute BuildStringAtt(uint16_t a_id, char *value, uint8_t size, uint8_t a_type)
{
    uint8_t *value_t = (uint8_t *)value;
    return attribute(a_id, value_t, size, a_type, 0x01);
}

const attribute manuf_attr = BuildStringAtt(MANUFACTURER_ATTR, manufacturer, sizeof(manufacturer), ZCL_CHAR_STR);
const attribute model_attr = BuildStringAtt(MODEL_ATTR, model, sizeof(model), ZCL_CHAR_STR);
const attribute zcl_ver_attr = attribute(0x0000, const_cast<uint8_t *>(zcl_version), sizeof(zcl_version), ZCL_UINT8_T, 0x01);
const attribute power_src_attr = attribute(0x0000, const_cast<uint8_t *>(power_source), sizeof(power_source), ZCL_ENUM8, 0x01);

attribute basic_attr[]{
    manuf_attr,
    zcl_ver_attr,
    power_src_attr,
    model_attr};

attribute door1_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door2_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door3_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door4_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute temp_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(two_zero_byte), 2, ZCL_INT16_T}};

static Cluster basic_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, basic_attr, sizeof(basic_attr) / sizeof(*basic_attr)),
};
static Cluster door1_in_clusters[] = {
    Cluster(ON_OFF_CLUSTER_ID, door1_attr, sizeof(door1_attr) / sizeof(*door1_attr))};
static Cluster door2_in_clusters[] = {
    Cluster(ON_OFF_CLUSTER_ID, door2_attr, sizeof(door2_attr) / sizeof(*door2_attr))};
static Cluster door3_in_clusters[] = {
    Cluster(ON_OFF_CLUSTER_ID, door3_attr, sizeof(door3_attr) / sizeof(*door3_attr))};
static Cluster door4_in_clusters[] = {
    Cluster(ON_OFF_CLUSTER_ID, door4_attr, sizeof(door4_attr) / sizeof(*door4_attr))};
static Cluster t_in_clusters[] = {
    Cluster(TEMP_CLUSTER_ID, temp_attr, sizeof(temp_attr) / sizeof(*temp_attr))};

static Cluster out_clusters[] = {};

static Endpoint ENDPOINTS[NUM_ENDPOINTS] = {
    Endpoint(1, ON_OFF_OUTPUT, basic_in_clusters, out_clusters, sizeof(basic_in_clusters) / sizeof(*basic_in_clusters), 0),
    Endpoint(2, ON_OFF_OUTPUT, door1_in_clusters, out_clusters, sizeof(door1_in_clusters) / sizeof(*door1_in_clusters), 0),
    Endpoint(3, ON_OFF_OUTPUT, door2_in_clusters, out_clusters, sizeof(door2_in_clusters) / sizeof(*door2_in_clusters), 0),
    Endpoint(4, ON_OFF_OUTPUT, door3_in_clusters, out_clusters, sizeof(door3_in_clusters) / sizeof(*door3_in_clusters), 0),
    Endpoint(5, ON_OFF_OUTPUT, door4_in_clusters, out_clusters, sizeof(door4_in_clusters) / sizeof(*door4_in_clusters), 0),
    Endpoint(6, TEMPERATURE_SENSOR, t_in_clusters, out_clusters, sizeof(t_in_clusters) / sizeof(*t_in_clusters), 0),
};
