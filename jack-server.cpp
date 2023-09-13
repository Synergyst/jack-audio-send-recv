#include <iostream>
#include <thread>
#include <jack/jack.h>
#include <boost/asio.hpp>
#include <chrono>
int curPort = 19977;
const int startingPort = curPort;
std::string curClient = "SRV";
constexpr int EXPECTED_AUDIO_BUFFER_SIZE = 1024 * sizeof(float);
template <typename T1, typename T2, typename T3>
class AudioServer {
public:
    AudioServer(T1 port, T2 jackServerName, T3 socket) : port(port), jackServerName(jackServerName), socket(socket) { }
    void run() {
        if (InitializeJack() != 0) {
            fprintf(stderr, "Failed to initialize JACK\n");
            return;
        }
    }
    void stop() {
        deinitJack();
    }
private:
    T1 port;
    T2 jackServerName;
    T3 socket;
    jack_client_t *client;
    jack_port_t *outputPort, *inputPort;
    void deinitJack() {
        // Cleanup and close JACK client
        jack_deactivate(client);
        jack_client_close(client);
    }
    int InitializeJack() {
        jack_status_t status;
        client = jack_client_open(jackServerName.c_str(), JackNullOption, &status);
        if (client == nullptr) {
            fprintf(stderr, "Failed to open JACK client\n");
            return -1;
        }
        // Register JACK ports and set the process callback.
        inputPort = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        outputPort = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (jack_set_process_callback(client, AudioCallback, this) != 0) {
            fprintf(stderr, "Failed to set JACK process callback for client\n");
            jack_client_close(client);
            return -1;
        }
        if (jack_activate(client) != 0) {
            fprintf(stderr, "Failed to activate JACK for client\n");
            jack_client_close(client);
            return -1;
        }
        // Automatically connect ports (add your own connections).
        jack_connect(client, "at2020_input:capture_1", jack_port_name(inputPort));
        jack_connect(client, jack_port_name(outputPort), "assound1_output:playback_1");
        fprintf(stderr, "JACK audio backend initialized, running now.\n");
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
            size_t bytesSent = audioServer->socket->send(boost::asio::buffer((float*)in, dataSize));
            if (bytesSent != EXPECTED_AUDIO_BUFFER_SIZE)
                fprintf(stderr, "send under-run: %d\n", dataSize);
        } catch (const boost::system::system_error &e) {
            std::string errorMessage = e.what();
            if (errorMessage.find("Bad file descriptor") != std::string::npos) {
                return 0;
            } else {
                fprintf(stderr, "Error while receiving from socket in client_in: %s\n", e.what());
                audioServer->deinitJack();
                return 0;
            }
        }
        try {
            size_t bytesRead = audioServer->socket->receive(boost::asio::buffer(out_buffer, dataSize));
            std::memcpy(out, out_buffer, bytesRead);
            if (bytesRead != EXPECTED_AUDIO_BUFFER_SIZE)
                fprintf(stderr, "recv under-run: %d\n", bytesRead);
        } catch (const boost::system::system_error &e) {
            std::string errorMessage = e.what();
            if (errorMessage.find("Bad file descriptor") != std::string::npos) {
                return 0;
            } else {
                fprintf(stderr, "Error while receiving from socket in client_out: %s\n", e.what());
                audioServer->deinitJack();
                return 0;
            }
        }
        return 0;
    }
};
// Setup JACK audio backend and TCP server
int TCPClientHandler(boost::asio::ip::tcp::socket socket) {
    fprintf(stderr, "Client connected (%s:%d)\n", socket.remote_endpoint().address().to_string().c_str(), socket.remote_endpoint().port());
    AudioServer<int, std::string, boost::asio::ip::tcp::socket*> obj1(curPort, curClient + "_" + std::to_string(curPort), &socket);
    obj1.run();
    curPort++;
    try {
        while (true) {
            float buffer[EXPECTED_AUDIO_BUFFER_SIZE];
            // Read from the socket without clearing the data
            boost::asio::mutable_buffers_1 customBuffer = boost::asio::buffer(buffer, EXPECTED_AUDIO_BUFFER_SIZE);
            boost::system::error_code ec;
            // Check if the client has disconnected
            size_t bytesRead = boost::asio::read(socket, customBuffer, ec);
            if (ec == boost::asio::error::eof) {
                fprintf(stderr, "Client disconnected (%s:%d)\n", socket.remote_endpoint().address().to_string().c_str(), socket.remote_endpoint().port());
                break;
            } else if (ec) {
                fprintf(stderr, "Error during data receive: %s\n", ec.message());
                break;
            }
            // Handle incoming data from the client here. You can use socket.read() or other async read operations. For simplicity, we just sleep for a short time in this example.
            std::this_thread::sleep_for(std::chrono::milliseconds(120000));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Exception in client handler: %s\n", e.what());
    }
    return 0;
}
int main() {
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::acceptor acceptor(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), startingPort));
    boost::asio::ip::tcp::socket socket(ioService);
    while (true) {
        //boost::asio::ip::tcp::socket socket(ioService);
        fprintf(stderr, "Awaiting for new client on: 0.0.0.0:%d\n", startingPort);
        acceptor.accept(socket);
        std::thread(TCPClientHandler, std::move(socket)).detach();
    }
    std::this_thread::sleep_for(std::chrono::seconds(20));
    return 0;
}
