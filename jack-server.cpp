#include <iostream>
#include <thread>
#include <jack/jack.h>
#include <boost/asio.hpp>
#include <chrono>
constexpr int EXPECTED_AUDIO_BUFFER_SIZE = 1024 * sizeof(float);
template <typename T1, typename T2>
class AudioServer {
public:
    AudioServer(T1 port, T2 jackServerName) : port(port), jackServerName(jackServerName) {}
    void run() {
        if (InitializeJack() != 0) {
            std::cerr << "Failed to initialize JACK" << std::endl;
            return;
        }
    }
private:
    T1 port;
    T2 jackServerName;
    jack_client_t *client;
    jack_port_t *outputPort, *inputPort;
    int InitializeJack() {
        jack_status_t status;
        client = jack_client_open(jackServerName.c_str(), JackNullOption, &status);
        if (client == nullptr) {
            std::cerr << "Failed to open JACK client" << std::endl;
            return -1;
        }
        // Register JACK ports and set the process callback.
        inputPort = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        outputPort = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (jack_set_process_callback(client, AudioCallback, this) != 0) {
            std::cerr << "Failed to set JACK process callback for client" << std::endl;
            jack_client_close(client);
            return -1;
        }
        if (jack_activate(client) != 0) {
            std::cerr << "Failed to activate JACK for client" << std::endl;
            jack_client_close(client);
            return -1;
        }
        // Automatically connect ports (add your own connections).
        jack_connect(client, "at2020_input:capture_1", jack_port_name(inputPort));
        jack_connect(client, jack_port_name(outputPort), "assound1_output:playback_1");
        std::cout << "JACK audio backend initialized, running now." << std::endl;
        return 0;
    }
    static int AudioCallback(jack_nframes_t nframes, void *arg) {
        AudioServer* audioServer = static_cast<AudioServer*>(arg);
        //boost::asio::ip::tcp::socket *socket = audioServer->socket;
        jack_default_audio_sample_t* in = (jack_default_audio_sample_t*)jack_port_get_buffer(audioServer->inputPort, nframes);
        jack_default_audio_sample_t* out = (jack_default_audio_sample_t*)jack_port_get_buffer(audioServer->outputPort, nframes);
        float out_buffer[EXPECTED_AUDIO_BUFFER_SIZE];
        size_t dataSize = nframes * sizeof(float);
        try {
            /*size_t bytesSent = socket->send(boost::asio::buffer((float*)in, dataSize));
            if (bytesSent != EXPECTED_AUDIO_BUFFER_SIZE)
                fprintf(stderr, "send under-run: %d\n", dataSize);*/
        } catch (const boost::system::system_error &e) {
            std::string errorMessage = e.what();
            if (errorMessage.find("Bad file descriptor") != std::string::npos) {
                return 0;
            } else {
                fprintf(stderr, "Error while receiving from socket in client_in: %s\n", e.what());
                return 0;
            }
        }
        try {
            /*size_t bytesRead = socket->receive(boost::asio::buffer(out_buffer, dataSize));
            std::memcpy(out, out_buffer, bytesRead);
            if (bytesRead != EXPECTED_AUDIO_BUFFER_SIZE)
                fprintf(stderr, "recv under-run: %d\n", bytesRead);*/
        } catch (const boost::system::system_error &e) {
            std::string errorMessage = e.what();
            if (errorMessage.find("Bad file descriptor") != std::string::npos) {
                return 0;
            } else {
                fprintf(stderr, "Error while receiving from socket in client_out: %s\n", e.what());
                return 0;
            }
        }
        return 0;
    }
};
int main() {
    int curPort = 19977;
    std::string curClient = "AudioServer";
    // Create multiple instances of the class
    AudioServer<int, std::string> obj1(curPort, curClient + "_" + std::to_string(curPort));
    curPort++;
    AudioServer<int, std::string> obj2(curPort, curClient + "_" + std::to_string(curPort));
    obj1.run();
    obj2.run();
    std::this_thread::sleep_for(std::chrono::seconds(20));
    return 0;
}
