lumizedmxengine: build/main.o build/parse_config.o
	g++ -pthread build/main.o build/parse_config.o /usr/local/lib/libpaho-mqtt3as.so /usr/local/lib/libpaho-mqtt3c.so /usr/local/lib/libpaho-mqttpp3.so -pthread -I/usr/local/include -L/usr/local/lib -lola -lolacommon -lprotobuf -pthread -o lumizedmxengine
build/main.o: src/main.cpp
	g++ -c src/main.cpp -o build/main.o
build/parse_config.o: src/parse_config.cpp
	g++ -c src/parse_config.cpp -o build/parse_config.o
clean: 
	rm -rf build/* lumizedmxengine
