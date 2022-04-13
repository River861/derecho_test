ALL: main.cpp aggregate_bandwidth.cpp
	g++ -std=c++1z -o main main.cpp aggregate_bandwidth.cpp -DUSE_VERBS_API=1 -lderecho -lcrypto -pthread

TEST: repeated_rpc_test.cpp
	g++ -std=c++1z -o main repeated_rpc_test.cpp -DUSE_VERBS_API=1 -lderecho -lcrypto -pthread

clean:
	rm main
