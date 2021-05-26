// main.cpp
// Lumize DMX Engine

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include "mqtt/client.h"
#include "nlohmann/json.hpp"
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

using namespace std;
using namespace std::chrono;

using json = nlohmann::json;

// CONFIG VARIABLES //
const string SERVER_ADDRESS{"tcp://192.168.1.61:1883"};
const string CLIENT_ID{"paho_cpp_sync_consume"};
const string USER_NAME{"sergio"};
const string PASSWORD{"sergio06"};
const string BASE_TOPIC{"mansardalight"};
const int CHANNELS = 20;
const float DEFAULT_TRANSITION = 1; // (s)
const int RENDER_FPS = 40;
const bool LOG_FADES = true;
const unsigned int DMX_UNIVERSE = 1;
// END CONFIG VARIABLES //

// INTERNAL CONFIG VARIABLES //
int initial_connection_retry_delay = 5;
const int initial_connection_retry_delay_max = 10;
const int initial_connection_retries_max = 20;
// END INTERNAL CONFIG VARIABLES //

int initial_connection_retries = 0;

// Init statekeeper arrays
int haLightBright[512];
bool haLightState[512];
float curLightBright[512];
float fadeDelta[512];
int fadeTarget[512];

bool lightProcessorRun = true;
bool programRun = true;

int decri = 0;

const string topic_delimiter = "/";

int frame_send_error_count = 0;
const int frame_send_error_max = 100;

/////////////////////////////////////////////////////////////////////////////

// Fill the arrays with appropriate values
void generateStatekeepers()
{
	for (int i = 0; i <= 512; i++)
	{
		haLightBright[i] = 255;
		haLightState[i] = false;
		curLightBright[i] = 0;
		fadeDelta[i] = 0;
		fadeTarget[i] = 0;
	}
}

// Return true if string is a number
bool isNumber(const std::string &s)
{
	return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// This function gets run in a separate thread and is responsible for computing and outputing each light frame
void lightProcessor()
{
	ola::DmxBuffer buffer; // A DmxBuffer to hold the data.
	buffer.Blackout();	   // Set all channels to 0
						   // Create a new client.
	ola::client::StreamingClient ola_client(
		(ola::client::StreamingClient::Options()));
	// Setup the client, this connects to the server
	if (!ola_client.Setup())
	{
		std::cerr << "[DMX]  Ola client setup failed! (check olad status)" << endl;
		lightProcessorRun = false;
		programRun = false;
	}

	cout << "[DMX] Ola client setup successful!" << endl;
	cout << "[DMX] Starting light output" << endl;

	while (lightProcessorRun)
	{

		// Render the current DMX frame
		for (int i = 0; i < 512; i++)
		{
			if (fadeDelta[i] > 0)
			{
				curLightBright[i] = curLightBright[i] + fadeDelta[i];

				if (curLightBright[i] >= fadeTarget[i])
				{
					curLightBright[i] = fadeTarget[i];
					fadeDelta[i] = 0;

					if (LOG_FADES)
						cout << "[LIGHT] Finished fade on CH" << i + 1 << endl;
				}
			}
			else if (fadeDelta[i] < 0)
			{
				curLightBright[i] = curLightBright[i] + fadeDelta[i];

				if (curLightBright[i] <= fadeTarget[i])
				{
					curLightBright[i] = fadeTarget[i];
					fadeDelta[i] = 0;

					if (LOG_FADES)
						cout << "[LIGHT] Finished fade on CH" << i + 1 << endl;
				}
			}

			buffer.SetChannel(i, curLightBright[i]); // Put value in DMX frame-buffer
		}

		// Send the frame to DMX
		if (!ola_client.SendDmx(DMX_UNIVERSE, buffer))
		{
			cout << "[DMX] DMX frame send failed!" << endl;

			if (frame_send_error_count > frame_send_error_max)
			{
				lightProcessorRun = false;
				programRun = false;
				exit(1);
			}

			frame_send_error_count++;
		}

		// for (int i = 0; i <= 20; i++)
		// {
		// 	cout << curLightBright[i] << "|";
		// }
		// cout << endl;

		this_thread::sleep_for(milliseconds(25));
	}
}

// Parse topic to obtain the light to be modified
int getLightIdFromTopic(string topic)
{
	size_t pos = 0;
	int i = 0;
	std::string token;
	int light_id = 512;
	while ((pos = topic.find(topic_delimiter)) != std::string::npos)
	{
		token = topic.substr(0, pos);
		if (i == 1)
		{
			if (isNumber(token))
			{
				light_id = stoi(token);
			}
		}
		topic.erase(0, pos + topic_delimiter.length());
		i++;
	}
	return light_id;
}

// Handle setting lights from mqtt messages
void messageReceived(string msg_json, string topic)
{
	try
	{
		int light_id = getLightIdFromTopic(topic);
		json msg = json::parse(msg_json);
		if (msg.contains("state"))
		{
			string state = msg["state"];
			state.erase(std::remove(state.begin(), state.end(), '\"'), state.end());

			float transition = (msg.contains("transition") ? float(msg["transition"]) : DEFAULT_TRANSITION);

			if (transition != 0)
			{
				if (state == "ON")
				{
					if (msg.contains("brightness"))
						haLightBright[light_id] = int(msg["brightness"]);

					fadeTarget[light_id] = int(haLightBright[light_id]);

					if (fadeTarget[light_id] != curLightBright[light_id])
					{
						fadeDelta[light_id] = float(fadeTarget[light_id] - curLightBright[light_id]) /
											  float(transition * RENDER_FPS);

						haLightState[light_id] = true;
						if (haLightBright[light_id] == 0)
							haLightState[light_id] = false;

						if (LOG_FADES)
							cout << "[LIGHT] Started fade on CH" << light_id
								 << " from " << curLightBright[light_id] << " to "
								 << fadeTarget[light_id] << ", delta " << fadeDelta[light_id] << endl;
					}
				}
				else if (state == "OFF")
				{
					if (curLightBright[light_id] != 0)
					{
						fadeTarget[light_id] = 0;
						fadeDelta[light_id] = float(fadeTarget[light_id] - curLightBright[light_id]) /
											  float(transition * RENDER_FPS);
						haLightState[light_id] = false;

						if (LOG_FADES)
							cout << "[LIGHT] Set CH" << light_id
								 << " to " << fadeTarget[light_id] << endl;
					}
				}
			}
			else
			{
				if (state == "ON")
				{
					if (msg.contains("brightness"))
						haLightBright[light_id] = int(msg["brightness"]);

					fadeTarget[light_id] = int(haLightBright[light_id]);

					if (fadeTarget[light_id] != curLightBright[light_id])
					{
						curLightBright[light_id] = fadeTarget[light_id];

						haLightState[light_id] = true;
						if (haLightBright[light_id] == 0)
							haLightState[light_id] = false;

						if (LOG_FADES)
							cout << "[FADE] Started on CH" << light_id
								 << " from " << curLightBright[light_id] << " to "
								 << fadeTarget[light_id] << ", delta " << fadeDelta[light_id] << endl;
					}
				}
				else if (state == "OFF")
				{
					if (curLightBright[light_id] != 0)
					{
						fadeTarget[light_id] = 0;
						fadeDelta[light_id] = float(fadeTarget[light_id] - curLightBright[light_id]) /
											  float(transition * RENDER_FPS);
						haLightState[light_id] = false;

						if (LOG_FADES)
							cout << "[FADE] Started on CH" << light_id
								 << " from " << curLightBright[light_id] << " to "
								 << fadeTarget[light_id] << ", delta " << fadeDelta[light_id] << endl;
					}
				}
			}
		}
	}
	catch (...)
	{
		cout << "[ERROR] Unable to parse incoming message: " << msg_json << endl;
	}
}

int main(int argc, char *argv[])
{
	cout << "### WELCOME TO THE LUMIZE DMX ENGINE! ###" << endl;
	cout << "Initiating startup procedure..." << endl;

	generateStatekeepers();

	// Start Light processor
	lightProcessorRun = true;
	std::thread light_processor_thread(lightProcessor);

	mqtt::client cli(SERVER_ADDRESS, CLIENT_ID); // Create new MQTT Client]

	// Set MQTT connction options
	auto connOpts = mqtt::connect_options_builder()
						.user_name(USER_NAME)
						.password(PASSWORD)
						.keep_alive_interval(seconds(30))
						.automatic_reconnect(seconds(2), seconds(30))
						.clean_session(false)
						.finalize();

	//  Set topics to subscribe to and respective QOS
	// const vector<string> TOPICS{"data/#", "command"};
	// const vector<int> QOS{0, 1};

	// Attempt connection to the MQTT broker. Retry connection multiple times because the auto reconnect featue doesn't
	// cover the initial connection attempt.
	// This is in case Home Assistant hasn't started yet when the DMX engine starts.

	while (!cli.is_connected())
	{
		try
		{
			cout << "[MQTT] Attemtping connection to mqtt server...";

			mqtt::connect_response rsp = cli.connect(connOpts);
			cout << "OK" << endl;

			// Subscribe to necessary topics
			std::cout << "[MQTT] Subscribing to topics..." << std::flush;
			for (int i = 0; i < CHANNELS; i++)
			{
				cli.subscribe(BASE_TOPIC + "/" + to_string(i) + "/set", 2);
			}
			std::cout << "OK" << std::endl;

			// Send "online" message
			cli.publish(BASE_TOPIC + "/avail", "online", strlen("online"), 0, true);

			// if (!rsp.is_session_present())
			// {
			// }
			// else
			// {
			// 	cout << "[MQTT] Session already present. Skipping subscribe." << std::endl;
			// }
		}
		// Catch errors in case the server is not ready to accept connections
		catch (const mqtt::exception &exc)
		{
			cerr << exc.what() << endl;
			cout << "[MQTT] There was an error connecting to the MQTT broker." << endl;
			cout << "[MQTT] Retrying in " << initial_connection_retry_delay << " seconds." << endl;
			initial_connection_retries++;
			if (initial_connection_retries >= initial_connection_retries_max)
			{
				cout << "Too many connection retries. Exiting." << endl;
				lightProcessorRun = false;
				light_processor_thread.join();
				return 1;
			}
			this_thread::sleep_for(seconds(5));
			if (initial_connection_retry_delay <= initial_connection_retry_delay_max)
				initial_connection_retry_delay++;
		}
	}

	while (programRun)
	{
		auto msg = cli.consume_message();

		if (msg)
		{
			// cout << msg->get_topic() << ": " << msg->to_string() << endl;
			messageReceived(msg->to_string(), msg->get_topic());
		}
		else if (!cli.is_connected())
		{
			cout << "[MQTT] Lost connection!" << endl;
			while (!cli.is_connected())
			{
				this_thread::sleep_for(milliseconds(250));
			}
			cout << "[MQTT] Re-established connection" << endl;
		}
	}

	cout << "[MQTT] Disconnecting from the MQTT broker..." << flush;
	cli.disconnect();
	cout << "OK" << endl;
	lightProcessorRun = false;
	light_processor_thread.join();
	return 0;
}

/* Add file status saving function
Every time it sends an acknowledge back to home assistant it can set the lights accordingly
maybe do it on external usb not to wear out the sd card */
