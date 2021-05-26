# Lumize DMX Engine
A piece of software which controls up to 100 channels of dmx dimmers, under control of a smart home hub such as Home Assistnat
## Requirements
There are some requirements that require extnernal build and install:
- Open Lighting Architecture [here](https://github.com/OpenLightingProject/ola)
- Paho MQTT Client C++ [here](https://github.com/eclipse/paho.mqtt.cpp)
## Build
The project can be downloaded from this repo and then built with the Makefile:


Just clone the repo

```
git clone https://github.com/carmisergio/lumize-dmx-engine.git
```


Then cd into the folder and build

```
cd lumize-dmx-engine
make
```

You will then find a `lumizedmxengine` file in your folder. Just make it executable and it's ready to go

```
chmod +x lumizedmxengine
./lumizedmxengine
```
## MQTT protocol definition
The DMX Engine is controlled through MQTT.
Il will respond to commands sent to 
```
[BASE_TOPIC]/[CHANNEL]/set
```
And it will publish state information on
```
[BASE_TOPIC]/[CHANNEL]/set
```
Commands and states have to follow the [Home Assitant MQTT Json Schema](https://www.home-assistant.io/integrations/light.mqtt/#json-schema)

Available properties are:
- `state`
- `brightness`
- `transition`
---
The status availabilty messages are an exception to this rule:

On startup, the program sends a payload of `online` to
```
[BASE_TOPIC]/avail
```

It also sets an LWT payload of `offline` to the same topic. This command is sent when this client disconnects from the MQTT broker.
