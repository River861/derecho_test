ALL: main.cpp aggregate_bandwidth.cpp
	g++ -std=c++1z -o main main.cpp aggregate_bandwidth.cpp -lcrypto -lderecho -pthread

clean:
	rm main
