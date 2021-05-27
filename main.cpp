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
#include "mqtt/async_client.h"
#include "nlohmann/json.hpp"
#include <ola/DmxBuffer.h>
#include <ola/io/SelectServer.h>
#include <ola/Logging.h>
#include <ola/client/ClientWrapper.h>
#include <ola/Callback.h>

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
const int N_RETRY_ATTEMPTS = 5;
// END CONFIG VARIABLES //

// INTERNAL CONFIG VARIABLES //
const char *LWT_SUBTOPIC = "/avail";
const char *LWT_PAYLOAD = "offline";
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

milliseconds fadeTimer[512];

bool lightProcessorRun = true;
bool programRun = true;

int decri = 0;

const string topic_delimiter = "/";

int frame_send_error_count = 0;
const int frame_send_error_max = 100;

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.

class action_listener : public virtual mqtt::iaction_listener
{
	std::string name_;

	void on_failure(const mqtt::token &tok) override
	{
		std::cout << name_ << " failure";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		std::cout << std::endl;
	}

	void on_success(const mqtt::token &tok) override
	{
		std::cout << name_ << " success";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		auto top = tok.get_topics();
		if (top && !top->empty())
			std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
		std::cout << std::endl;
	}

public:
	action_listener(const std::string &name) : name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
				 public virtual mqtt::iaction_listener

{
	// Counter for the number of connection retries
	int nretry_;
	// The MQTT client
	mqtt::async_client &cli_;
	// Options to use if we need to reconnect
	mqtt::connect_options &connOpts_;
	// An action listener to display the result of actions.
	action_listener subListener_;

	// This deomonstrates manually reconnecting to the broker by calling
	// connect() again. This is a possibility for an application that keeps
	// a copy of it's original connect_options, or if the app wants to
	// reconnect with different options.
	// Another way this can be done manually, if using the same options, is
	// to just call the async_client::reconnect() method.
	void reconnect()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		try
		{
			cli_.connect(connOpts_, nullptr, *this);
		}
		catch (const mqtt::exception &exc)
		{
			std::cerr << "[MQTT] Error: " << exc.what() << std::endl;
			exit(1);
		}
	}

	// Re-connection failure
	void on_failure(const mqtt::token &tok) override
	{
		std::cout << "[MQTT] Connection attempt failed" << std::endl;
		if (++nretry_ > N_RETRY_ATTEMPTS)
			exit(1);
		reconnect();
	}

	// (Re)connection success
	// Either this or connected() can be used for callbacks.
	void on_success(const mqtt::token &tok) override {}

	// (Re)connection success
	void connected(const std::string &cause) override
	{
		std::cout << "[MQTT] Connected!" << std::endl;
		// Subscribe to necessary topics
		std::cout << "[MQTT] Subscribing to topics..." << std::flush;
		for (int i = 0; i < CHANNELS; i++)
		{
			cli_.subscribe(BASE_TOPIC + "/" + to_string(i) + "/set", 2);
		}
		std::cout << "OK" << std::endl;

		// Send "online" message
		cli_.publish(BASE_TOPIC + "/avail", "online", strlen("online"), 0, true);
		for (int i = 0; i < CHANNELS; i++)
		{
			publishLightState(i);
		}
	}

	// Callback for when the connection is lost.
	// This will initiate the attempt to manually reconnect.
	void connection_lost(const std::string &cause) override
	{
		std::cout << "[MQTT] Connection lost. " << std::endl;
		if (!cause.empty())
			std::cout << "[MQTT] Cause: " << cause << std::endl;

		std::cout << "[MQTT] Reconnecting..." << std::endl;
		nretry_ = 0;
		reconnect();
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

	// Return true if string is a number
	bool isNumber(const std::string &s)
	{
		return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
	}

	// Callback for when a message arrives.
	void message_arrived(mqtt::const_message_ptr msgptr) override
	{
		string msg_json = msgptr->to_string();
		string topic = msgptr->get_topic();
		try
		{
			int light_id = getLightIdFromTopic(topic);

			// Parse json
			json msg = json::parse(msg_json);
			if (msg.contains("state"))
			{
				string state = msg["state"];
				state.erase(std::remove(state.begin(), state.end(), '\"'), state.end());

				// Set the transition to either the received one if there is, otherwise the default transition
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
				publishLightState(light_id);
			}
		}
		catch (...)
		{
			cout << "[ERROR] Unable to parse incoming message: " << msg_json << endl;
		}
	}

	// Public state of selected light from the haLighState and haLightBright
	void publishLightState(const int light_id)
	{
		string state_on_off = (haLightState[light_id]) ? "ON" : "OFF";
		json data;
		data["state"] = state_on_off;
		data["brightness"] = haLightBright[light_id];
		string json_data = data.dump();
		string topic = BASE_TOPIC + '/' + to_string(light_id);
		const char *json_data_char = json_data.c_str();
		cli_.publish(topic, json_data_char, strlen(json_data_char), 0, true);
	}

	void delivery_complete(mqtt::delivery_token_ptr token) override
	{
	}

public:
	callback(mqtt::async_client &cli, mqtt::connect_options &connOpts)
		: nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
};

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
		fadeTimer[i] = duration_cast<milliseconds>(
			system_clock::now().time_since_epoch());
	}
	// Don't know why this is needed but if not here the first value is always true...
	haLightState[0] = false;
}

bool SendData(ola::client::OlaClientWrapper *wrapper)
{
	ola::DmxBuffer buffer; // A DmxBuffer to hold the data.
	buffer.Blackout();	   // Set all channels to 0

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
	wrapper->GetClient()->SendDMX(DMX_UNIVERSE, buffer, ola::client::SendDMXArgs());

	if (!lightProcessorRun)
		wrapper->GetSelectServer()->Terminate();

	return true;
}

// This function gets run in a separate thread and is responsible for computing and outputing each light frame
void lightProcessor()
{
	// Create a new client.
	ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);
	ola::client::OlaClientWrapper wrapper;
	// Setup the client, this connects to the server
	if (!wrapper.Setup())
	{
		std::cerr << "[DMX]  Ola client setup failed! (check olad status)" << endl;
		lightProcessorRun = false;
		programRun = false;
	}

	// Create a timeout and register it with the SelectServer
	ola::io::SelectServer *ss = wrapper.GetSelectServer();
	ss->RegisterRepeatingTimeout(1000 / RENDER_FPS, ola::NewCallback(&SendData, &wrapper));

	cout << "[DMX] Ola client setup successful!" << endl;
	cout << "[DMX] Starting light output..." << endl;

	// Start the main loop
	ss->Run();
}

int main(int argc, char *argv[])
{
	cout << "### WELCOME TO THE LUMIZE DMX ENGINE! ###" << endl;
	cout << "Initiating startup procedure..." << endl;

	generateStatekeepers();

	// Start Light processor
	lightProcessorRun = true;
	std::thread light_processor_thread(lightProcessor);

	mqtt::async_client cli(SERVER_ADDRESS, CLIENT_ID);

	// Compute lwt topic
	string temp_lwt_topic = BASE_TOPIC;
	temp_lwt_topic += LWT_SUBTOPIC;
	const char *lwt_topic = temp_lwt_topic.c_str();

	// Set MQTT connction options
	auto connOpts = mqtt::connect_options_builder()
						.user_name(USER_NAME)
						.password(PASSWORD)
						.keep_alive_interval(seconds(30))
						.automatic_reconnect(seconds(2), seconds(30))
						.clean_session(false)
						.will(mqtt::message(BASE_TOPIC + "/avail", "offline", strlen("offline"), 0, true))
						.finalize();

	// Install the callback(s) before connecting.
	callback cb(cli, connOpts);
	cli.set_callback(cb);

	// Start the connection.
	// When completed, the callback will subscribe to topic.

	try
	{
		cout << "[MQTT] Attemtping connection to mqtt server..." << endl;
		cli.connect(connOpts, nullptr, cb);
	}
	catch (const mqtt::exception &exc)
	{
		std::cerr << "[MQTT] There was an error connecting to the MQTT broker."
				  << SERVER_ADDRESS << "'" << exc << std::endl;
		this_thread::sleep_for(seconds(5));
		programRun = false;
	}

	// Block until exit
	while (programRun)
	{
		this_thread::sleep_for(milliseconds(100));
	}

	// Disconnect

	try
	{
		std::cout << "[MQTT] Disconnecting from the MQTT broker..." << std::flush;
		cli.disconnect()->wait();
		std::cout << "OK" << std::endl;
		lightProcessorRun = false;
		light_processor_thread.join();
		return 0;
	}
	catch (const mqtt::exception &exc)
	{
		std::cerr << exc << std::endl;
		lightProcessorRun = false;
		light_processor_thread.join();
		return 1;
	}
}
/* Add file status saving function
Every time it sends an acknowledge back to home assistant it can set the lights accordingly
maybe do it on external usb not to wear out the sd card */
