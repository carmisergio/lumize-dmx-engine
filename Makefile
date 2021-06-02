lumizedmxengine: main.o parse_config.o
	g++ -pthread main.o parse_config.o /usr/local/lib/libpaho-mqtt3as.so /usr/local/lib/libpaho-mqtt3c.so /usr/local/lib/libpaho-mqttpp3.so -pthread -I/usr/local/include -L/usr/local/lib -lola -lolacommon -lprotobuf -pthread -o lumizedmxengine
main.o: main.cpp
	g++ -c main.cpp
parse_config.o: parse_config.cpp
	g++ -c parse_config.cpp
clean: 
	rm -f *.o lumizedmxengine
