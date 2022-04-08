ALL: simple_replicated_objects.cpp
	g++ -std=c++1z -o main main.cpp -lcrypto -lderecho -pthread

clean:
	rm main
