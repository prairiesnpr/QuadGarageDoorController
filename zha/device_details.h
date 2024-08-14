#include <stdint.h>

#define NUM_ENDPOINTS 5

constexpr uint8_t one_zero_byte[] = {0x00};
constexpr uint8_t one_max_byte[] = {0xFF};
constexpr uint8_t two_zero_byte[] = {0x00, 0x00};
constexpr uint8_t four_zero_byte[] = {0x00, 0x00, 0x00, 0x00};

// Reuse these to save SRAM
#define manufacturer "iSilentLLC"
#define door1_model "Door1"
#define door2_model "Door2"
#define door3_model "Door3"
#define door4_model "Door4"
#define temp_model "Temp"

attribute BuildStringAtt(uint16_t a_id, char *value, uint8_t size, uint8_t a_type)
{
    uint8_t *value_t = (uint8_t *)value;
    return attribute(a_id, value_t, size, a_type, 0x01);
}

const attribute manuf_attr = BuildStringAtt(MANUFACTURER_ATTR, manufacturer, sizeof(manufacturer), ZCL_CHAR_STR);
const attribute door1_model_attr = BuildStringAtt(MODEL_ATTR, door1_model, sizeof(door1_model), ZCL_CHAR_STR);
const attribute door2_model_attr = BuildStringAtt(MODEL_ATTR, door2_model, sizeof(door2_model), ZCL_CHAR_STR);
const attribute door3_model_attr = BuildStringAtt(MODEL_ATTR, door3_model, sizeof(door3_model), ZCL_CHAR_STR);
const attribute door4_model_attr = BuildStringAtt(MODEL_ATTR, door4_model, sizeof(door4_model), ZCL_CHAR_STR);
const attribute temp_model_attr = BuildStringAtt(MODEL_ATTR, temp_model, sizeof(temp_model), ZCL_CHAR_STR);






attribute temp_basic_attr[]{
    manuf_attr,
    temp_model_attr};
attribute door1_basic_attr[]{
    manuf_attr,
    door1_model_attr};
attribute door2_basic_attr[]{
    manuf_attr,
    door2_model_attr};
attribute door3_basic_attr[]{
    manuf_attr,
    door3_model_attr};
attribute door4_basic_attr[]{
    manuf_attr,
    door4_model_attr};

attribute door1_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door2_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door3_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute door4_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(one_zero_byte), 1, ZCL_BOOL}};
attribute temp_attr[] = {{CURRENT_STATE, const_cast<uint8_t *>(two_zero_byte), 2, ZCL_INT16_T}};


static Cluster door1_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, door1_basic_attr, sizeof(door1_basic_attr)/sizeof(*door1_basic_attr)),
    Cluster(ON_OFF_CLUSTER_ID, door1_attr, sizeof(door1_attr)/sizeof(*door1_attr))};
static Cluster door2_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, door2_basic_attr, sizeof(door2_basic_attr)/sizeof(*door2_basic_attr)),
    Cluster(ON_OFF_CLUSTER_ID, door2_attr, sizeof(door2_attr)/sizeof(*door2_attr))};
static Cluster door3_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, door3_basic_attr, sizeof(door3_basic_attr)/sizeof(*door3_basic_attr)),
    Cluster(ON_OFF_CLUSTER_ID, door3_attr, sizeof(door3_attr)/sizeof(*door3_attr))};
static Cluster door4_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, door4_basic_attr, sizeof(door4_basic_attr)/sizeof(*door4_basic_attr)),
    Cluster(ON_OFF_CLUSTER_ID, door4_attr, sizeof(door4_attr)/sizeof(*door4_attr))};
static Cluster t_in_clusters[] = {
    Cluster(BASIC_CLUSTER_ID, temp_basic_attr, sizeof(temp_basic_attr)/sizeof(*temp_basic_attr)),
    Cluster(TEMP_CLUSTER_ID, temp_attr, sizeof(temp_attr)/sizeof(*temp_attr))};

static Cluster out_clusters[] = {};
static Endpoint ENDPOINTS[NUM_ENDPOINTS] = {
    Endpoint(1, ON_OFF_OUTPUT, door1_in_clusters, out_clusters, sizeof(door1_in_clusters)/sizeof(*door1_in_clusters), 0),
    Endpoint(2, ON_OFF_OUTPUT, door2_in_clusters, out_clusters, sizeof(door2_in_clusters)/sizeof(*door2_in_clusters), 0),
    Endpoint(3, ON_OFF_OUTPUT, door3_in_clusters, out_clusters, sizeof(door3_in_clusters)/sizeof(*door3_in_clusters), 0),
    Endpoint(4, ON_OFF_OUTPUT, door4_in_clusters, out_clusters, sizeof(door4_in_clusters)/sizeof(*door4_in_clusters), 0),
    Endpoint(5, TEMPERATURE_SENSOR, t_in_clusters, out_clusters, sizeof(t_in_clusters)/sizeof(*t_in_clusters), 0),
};
