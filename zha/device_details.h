#include <stdint.h>

#define NUM_ENDPOINTS 5

static uint8_t *manuf = (uint8_t *)"iSilentLLC";
static attribute temp_basic_attr[]{
    {MANUFACTURER_ATTR, manuf, 10, ZCL_CHAR_STR},
    {MODEL_ATTR, (uint8_t *)"Temp", 4, ZCL_CHAR_STR}};
static attribute door1_basic_attr[]{
    {MANUFACTURER_ATTR, manuf, 10, ZCL_CHAR_STR},
    {MODEL_ATTR, (uint8_t *)"Door 1", 6, ZCL_CHAR_STR}};
static attribute door2_basic_attr[]{
    {MANUFACTURER_ATTR, manuf, 10, ZCL_CHAR_STR},
    {MODEL_ATTR, (uint8_t *)"Door 2", 6, ZCL_CHAR_STR}};
static attribute door3_basic_attr[]{
    {MANUFACTURER_ATTR, manuf, 10, ZCL_CHAR_STR},
    {MODEL_ATTR, (uint8_t *)"Door 3", 6, ZCL_CHAR_STR}};
static attribute door4_basic_attr[]{
    {MANUFACTURER_ATTR, manuf, 10, ZCL_CHAR_STR},
    {MODEL_ATTR, (uint8_t *)"Door 4", 6, ZCL_CHAR_STR}};
static attribute door1_attr[]{{CURRENT_STATE, 0x00, 1, ZCL_BOOL}};
static attribute door2_attr[]{{CURRENT_STATE, 0x00, 1, ZCL_BOOL}};
static attribute door3_attr[]{{CURRENT_STATE, 0x00, 1, ZCL_BOOL}};
static attribute door4_attr[]{{CURRENT_STATE, 0x00, 1, ZCL_BOOL}};
static attribute temp_attr[] = {
    {CURRENT_STATE, 0x00, 2, ZCL_INT16_T}};

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
