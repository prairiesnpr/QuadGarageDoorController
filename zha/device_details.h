#include <stdint.h>

#define NUM_ENDPOINTS 5

static uint8_t *manuf = (uint8_t *)"iSilentLLC";
static attribute temp_basic_attr[]{{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t *)"Temp", 4, ZCL_CHAR_STR}};
static attribute door1_basic_attr[]{{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t *)"Door 1", 6, ZCL_CHAR_STR}};
static attribute door2_basic_attr[]{{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t *)"Door 2", 6, ZCL_CHAR_STR}};
static attribute door3_basic_attr[]{{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t *)"Door 3", 6, ZCL_CHAR_STR}};
static attribute door4_basic_attr[]{{0x0004, manuf, 10, ZCL_CHAR_STR}, {0x0005, (uint8_t *)"Door 4", 6, ZCL_CHAR_STR}};
static attribute door1_attr[]{{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door2_attr[]{{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door3_attr[]{{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute door4_attr[]{{0x0000, 0x00, 1, ZCL_BOOL}};
static attribute temp_attr[] = {{0x0000, 0x00, 2, ZCL_INT16_T}};

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
