ALL: main.cpp
	g++ -std=c++1z -o main main.cpp -DUSE_VERBS_API=1 -lderecho -lcrypto -pthread

verbs: main.cpp
	g++ -std=c++1z -o main main.cpp -DUSE_VERBS_API=1 -lderecho -lcrypto -pthread

test: repeated_rpc_test.cpp
	g++ -std=c++1z -o main repeated_rpc_test.cpp -lderecho -lcrypto -pthread

bk: main_bk.cpp
	g++ -std=c++1z -o main main_bk.cpp -lderecho -lcrypto -pthread

clean:
	rm main
