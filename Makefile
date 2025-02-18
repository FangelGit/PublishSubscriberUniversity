CXX=g++
CXXFLAGS=-Wall -Wextra

TARGET_CLIENT = client_app
TARGET_SERVER = server_app

SOURCE_CLIENT = main.cpp clientlib.cpp
SOURCE_SERVER = server.cpp

HEADERS = clientlib.h

all: $(TARGET_CLIENT) $(TARGET_SERVER)

$(TARGET_CLIENT): $(CLIENT_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLIENT) $(SOURCE_CLIENT)

$(TARGET_SERVER): $(SERVER_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET_SERVER) $(SOURCE_SERVER)

clean:
	rm -f $(TARGET_CLIENT) $(TARGET_SERVER)
	rm -f *.o
