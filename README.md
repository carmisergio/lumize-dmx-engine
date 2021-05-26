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
```
