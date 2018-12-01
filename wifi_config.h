/*
 *
 * 
 */

typedef struct  {
    const char* ssid;
    const char* pwd;
} pp_wifi_network;

// Set this to the number of items in the array below.
#define NUM_WIFI_NETWORKS   (2)

pp_wifi_network wifi_networks[] = {
    {"xxSSIDxx", "xxPWDxx"}   // Network #1
//    ,{"xxSSID2xx", "xxPWD2xx"}  // Network #2
};

