#include <iostream>
#include <string>
#include <fstream>
#include "nlohmann/json.hpp"

#include "parse_config.hpp"

LumizeConfigParser::LumizeConfigParser(std::string file_path)
{
    std::cout << "[CONFIG] Parsing config file: " << file_path << std::endl;
    std::ifstream configFile;
    try
    {
        configFile.open(file_path);
        nlohmann::json config;
        configFile >> config;

        // Perform necessary checks on values
        if (config.contains("channels"))
        {
            channels = config["channels"];
            if (channels < 1 || channels > 100)
            {
                std::cout << "[CONFIG] Invaild channel number. Exiting" << std::endl;
                exit(1);
            }
        }

        if (config.contains("fps"))
        {
            fps = config["fps"];
            if (fps < 1 || fps > 200)
            {
                std::cout << "[CONFIG] Invaild render FPS. Exiting" << std::endl;
                exit(1);
            }
        }

        if (config.contains("default_transition"))
        {
            default_transition = config["default_transition"];
            if (!(default_transition > 0) || default_transition > 999)
            {
                std::cout << "[CONFIG] Invaild default transition. Exiting" << std::endl;
                exit(1);
            }
        }

        if (config.contains("broker host"))
        {
            host = config["broker host"];
        }

        if (config.contains("broker port"))
        {
            port = config["port"];
            if (port < 1 || port > 65535)
            {
                std::cout << "[CONFIG] Invaild broker port. Exiting" << std::endl;
                exit(1);
            }
        }

        if (config.contains("base topic"))
        {
            base_topic = config["base topic"];
        }

        if (config.contains("client id"))
        {
            client_id = config["client id"];
        }

        if (config.contains("log fades"))
        {
            log_fades = config["log fades"];
        }

        if (config.contains("universe"))
        {
            universe = config["universe"];
            if (universe < 0 || universe > 999)
            {
                std::cout << "[CONFIG] Invaild DMX universe. Exiting" << std::endl;
                exit(1);
            }
        }

        if (config.contains("user") && config.contains("password"))
        {
            user = config["user"];
            password = config["password"];
            auth = true;
        }
        else if (config.contains("user"))
        {
            user = config["user"];
            password = "";
            auth = true;
        }
        else if (config.contains("password"))
        {
            password = config["password"];
            user = "";
            auth = true;
        }

        if (host == "")
        {
            std::cout << "[CONFIG] Broker host must be specified!" << std::endl;
            exit(1);
        }
    }
    catch (...)
    {
        std::cout << "There was an error opening or parsing the config file." << std::endl;
        exit(1);
    }

    std::cout << "[CONFIG] Entries:" << std::endl;
    std::cout << "  - DMX channels: " << channels << std::endl;
    std::cout << "  - DMX universe: " << universe << std::endl;
    std::cout << "  - Render FPS: " << fps << std::endl;
    std::cout << "  - Default Transition: " << default_transition << std::endl;
    std::cout << "  - Broker Host: " << host << std::endl;
    std::cout << "  - Broker Port: " << port << std::endl;
    std::cout << "  - MQTT client_id: " << client_id << std::endl;
    std::cout << "  - Base Topic: " << base_topic << std::endl;
    std::cout << "  - Authtentication: " << ((auth) ? "enabled" : "disabled") << std::endl;
    if (auth)
    {
        std::cout << "  - User: " << user << std::endl;
        std::cout << "  - Password: " << password << std::endl;
    }
}