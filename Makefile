lumizedmxengine: main.o
	g++ -pthread main.o /usr/local/lib/libpaho-mqtt3as.so /usr/local/lib/libpaho-mqtt3c.so /usr/local/lib/libpaho-mqttpp3.so -pthread -I/usr/local/include -L/usr/local/lib -lola -lolacommon -lprotobuf -pthread -o lumizedmxengine
main.o: main.cpp
	g++ -c main.cpp
test: async_subscribe.cpp
	g++ -pthread async_subscribe.cpp /usr/local/lib/libpaho-mqtt3as.so /usr/local/lib/libpaho-mqtt3c.so /usr/local/lib/libpaho-mqttpp3.so -pthread -I/usr/local/include -L/usr/local/lib -lola -lolacommon -lprotobuf -pthread -o test
clean: 
	rm -f *.o lumizedmxengine
