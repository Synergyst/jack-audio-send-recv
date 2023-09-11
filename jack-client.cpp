#include <iostream>
#include <vector>
#include <thread>
#include <jack/jack.h>
#include <boost/asio.hpp>
#include <chrono>
const int SAMPLE_RATE = 48000;
const char* SERVER_ADDRESS = "192.168.168.175";
const int PORT = 19977;
constexpr int MAX_AUDIO_BUFFER_SIZE = 1024 * sizeof(float); // 4096
jack_client_t *client;
jack_port_t *outputPort, *inputPort;
int AudioCallback(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t* out = (jack_default_audio_sample_t*)jack_port_get_buffer(outputPort, nframes);
    jack_default_audio_sample_t* in = (jack_default_audio_sample_t*)jack_port_get_buffer(inputPort, nframes);
    boost::asio::ip::tcp::socket *socket = static_cast<boost::asio::ip::tcp::socket*>(arg); // Cast the user data to a socket pointer
    size_t dataSize = nframes * sizeof(float);
    float out_buffer[MAX_AUDIO_BUFFER_SIZE];
    try {
        size_t bytesSent = socket->send(boost::asio::buffer((float*)in, dataSize));
        if (bytesSent != MAX_AUDIO_BUFFER_SIZE)
            fprintf(stderr, "send: %d\n", dataSize);
    } catch (const boost::system::system_error &e) {
        fprintf(stderr, "Error while receiving from socket in client_in: %s\n", e.what());
        exit(1);
    }
    try {
        size_t bytesRead = socket->receive(boost::asio::buffer(out_buffer, dataSize));
        if (bytesRead != MAX_AUDIO_BUFFER_SIZE)
            fprintf(stderr, "recv: %d\n", bytesRead);
        std::memcpy(out, out_buffer, bytesRead);
    } catch (const boost::system::system_error &e) {
        fprintf(stderr, "Error while receiving from socket in client_out: %s\n", e.what());
        exit(1);
    }
    return 0;
}
int TCPClientHandler(boost::asio::ip::tcp::socket socket) {
    jack_status_t status;
    client = jack_client_open("AudioClient", JackNullOption, &status);
    if (client == nullptr) {
        fprintf(stderr, "Failed to open JACK client\n");
        return -1;
    }
    inputPort = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    outputPort = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (jack_set_process_callback(client, AudioCallback, &socket)) {
        fprintf(stderr, "Failed to set JACK process callback for client\n");
        jack_client_close(client);
        return -1;
    }
    if (jack_activate(client)) {
        fprintf(stderr, "Failed to activate JACK for client\n");
        jack_client_close(client);
        return -1;
    }
    // Automatically connect ports
    const char* inputPortName = jack_port_name(inputPort);
    const char* inputSource = "at2020_input:capture_1";
    const char* outputPortName = jack_port_name(outputPort);
    const char* outputTarget = "assound1_output:playback_1";
    //jack_connect(client_in, inputSource, inputPortName);
    //jack_connect(client_out, outputPortName, outputTarget);
    fprintf(stderr, "JACK audio backend initialized.. running now.\n");
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    // Cleanup and close JACK client
    jack_deactivate(client);
    jack_client_close(client);
    return 0;
}
int main(int argc, char *argv[]) {
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::socket socket(ioService);
    try {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(SERVER_ADDRESS), PORT);
        socket.connect(endpoint);
        fprintf(stderr, "Client connected to (%s:%d)\n", SERVER_ADDRESS, PORT);
    } catch (const boost::system::system_error &e) {
        fprintf(stderr, "Error while receiving from socket in client_out: %s\n", e.what());
        return -1;
    }
    std::thread(TCPClientHandler, std::move(socket)).detach();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}
