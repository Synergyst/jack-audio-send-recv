#include <iostream>
#include <vector>
#include <thread>
#include <jack/jack.h>
#include <boost/asio.hpp>
#include <chrono>

const int SAMPLE_RATE = 48000;
const char* SERVER_ADDRESS = "192.168.168.175";
const int PORT = 19976;
constexpr int MAX_AUDIO_BUFFER_SIZE = 1024 * sizeof(float); // 4096
std::mutex audioMutex;
jack_client_t *client_in, *client_out;
jack_port_t *outputPort, *inputPort;

// JACK audio callback function to capture audio from the microphone and send it to the TCP client
int AudioInputCallback(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t* in = (jack_default_audio_sample_t*)jack_port_get_buffer(inputPort, nframes);
    boost::asio::ip::tcp::socket *socket = static_cast<boost::asio::ip::tcp::socket*>(arg); // Cast the user data to a socket pointer
    // Capture audio from the microphone and send it to the TCP client
    size_t dataSize = nframes * sizeof(float);
    try {
        size_t bytesSent = socket->send(boost::asio::buffer((float*)in, dataSize));
        if (bytesSent != MAX_AUDIO_BUFFER_SIZE)
            fprintf(stderr, "s:%d\n", dataSize);
    } catch (const boost::system::system_error &e) {
        std::cerr << "Error while receiving from socket in client_in: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
// JACK audio callback function to receive audio from TCP client and play it to the speakers
int AudioOutputCallback(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t* out = (jack_default_audio_sample_t*)jack_port_get_buffer(outputPort, nframes);
    boost::asio::ip::tcp::socket *socket = static_cast<boost::asio::ip::tcp::socket*>(arg); // Cast the user data to a socket pointer
    // Receive audio data from the TCP client and play it
    size_t dataSize = nframes * sizeof(float);
    float out_buffer[MAX_AUDIO_BUFFER_SIZE];
    try {
        size_t bytesRead = socket->receive(boost::asio::buffer(out_buffer, dataSize));
        if (bytesRead != MAX_AUDIO_BUFFER_SIZE)
            fprintf(stderr, "r:%d\n", bytesRead);
        std::memcpy(out, out_buffer, bytesRead);
    } catch (const boost::system::system_error &e) {
        std::cerr << "Error while receiving from socket in client_out: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
int TCPClientHandler(boost::asio::ip::tcp::socket socket) {
    // Handle TCP client connection
    jack_status_t status_in, status_out;
    client_in = jack_client_open("AudioClient_in", JackNullOption, &status_in);
    client_out = jack_client_open("AudioClient_out", JackNullOption, &status_out);
    if (client_in == nullptr) {
        std::cerr << "Failed to open JACK client_in" << std::endl;
        return -1;
    }
    if (client_out == nullptr) {
        std::cerr << "Failed to open JACK client_out" << std::endl;
        return -1;
    }
    inputPort = jack_port_register(client_in, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    outputPort = jack_port_register(client_out, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (jack_set_process_callback(client_in, AudioInputCallback, &socket)) {
        std::cerr << "Failed to set JACK process callback for client_in" << std::endl;
        jack_client_close(client_in);
        return -1;
    }
    if (jack_activate(client_in)) {
        std::cerr << "Failed to activate JACK for client_in" << std::endl;
        jack_client_close(client_in);
        return -1;
    }
    if (jack_set_process_callback(client_out, AudioOutputCallback, &socket)) {
        std::cerr << "Failed to set JACK process callback for client_out" << std::endl;
        jack_client_close(client_out);
        return -1;
    }
    if (jack_activate(client_out)) {
        std::cerr << "Failed to activate JACK for client_out" << std::endl;
        jack_client_close(client_out);
        return -1;
    }
    // Automatically connect ports
    const char* inputPortName = jack_port_name(inputPort);
    const char* inputSource = "at2020_input:capture_1";
    const char* outputPortName = jack_port_name(outputPort);
    const char* outputTarget = "assound1_output:playback_1";
    //jack_connect(client_in, inputSource, inputPortName);
    //jack_connect(client_out, outputPortName, outputTarget);
    std::cout << "JACK audio backend initialized.. running now." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    // Cleanup and close JACK client
    jack_deactivate(client_in);
    jack_deactivate(client_out);
    jack_client_close(client_in);
    jack_client_close(client_out);
    return 0;
}
int main(int argc, char *argv[]) {
    // Create TCP client
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::socket socket(ioService);
    try {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(SERVER_ADDRESS), PORT);
        socket.connect(endpoint);
        fprintf(stderr, "Client connected to: %s:%d\n", SERVER_ADDRESS, PORT);
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
