all:client server

client:client.cpp $(filter-out %skeleton.cpp, $(wildcard gen-cpp/*.cpp))
	g++ -lthrift -lssl $^ -o $@

server:server.cpp
	

