#pragma once

#include <string>

//////////////////////////////// DEFAULT VALUES //////////////////////////////////////
// Light
#define DEFAULT_CHANNELS 20
#define DEFAULT_FPS 30
#define DEFAULT_DEFAULT_TRANSITION 1
//  MQTT
#define DEFAULT_PORT 1883
#define DEFAULT_CLIENT_ID "lumizedmxengine"
#define DEFAULT_AUTH false
#define DEFAULT_BASE_TOPIC "lumizedmxengine"
#define DEFAULT_LOG_FADES false
#define DEFAULT_UNIVERSE 1
//////////////////////////////// DEFAULT VALUES //////////////////////////////////////

class LumizeConfigParser
{
public:
    // Light stuff
    unsigned int channels = DEFAULT_CHANNELS;
    unsigned int fps = DEFAULT_FPS;
    float default_transition = DEFAULT_DEFAULT_TRANSITION;

    // MQTT stuff
    std::string host = "";
    int port = DEFAULT_PORT;
    std::string client_id = DEFAULT_CLIENT_ID;
    bool auth = DEFAULT_AUTH;
    std::string user;
    std::string password;
    std::string base_topic = DEFAULT_BASE_TOPIC;
    bool log_fades = DEFAULT_LOG_FADES;
    int universe = DEFAULT_UNIVERSE;

    LumizeConfigParser(std::string file_path);
};