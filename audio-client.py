import os,sys,socket
sys.path.append(os.getcwd())
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_address = ("192.168.168.175", 19977)
client_socket.connect(server_address)
if __name__ == '__main__':
    import numpy as np
    import traceback, re
    import json
    import PySimpleGUI as sg
    import sounddevice as sd
    import time, threading
    from i18n import I18nAuto
    i18n = I18nAuto()
    class GUIConfig:
        def __init__(self) -> None:
            self.samplerate: int = 40000
    class GUI:
        def __init__(self) -> None:
            self.config = GUIConfig()
            self.flag_vc = False
            self.launcher()
        def load(self):
            input_devices, output_devices, _, _ = self.get_devices()
            try:
                with open("values_sender.json", "r") as j:
                    data = json.load(j)
            except:
                with open("values_sender.json", "w") as j:
                    data = {"sg_input_device": input_devices[sd.default.device[0]], "sg_output_device": output_devices[sd.default.device[1]]}
            return data
        def launcher(self):
            data = self.load()
            sg.theme("DarkGreen3")
            input_devices, output_devices, _, _ = self.get_devices()
            layout = [
                [
                    sg.Frame(
                        layout=[
                            [
                                sg.Text(i18n("input device         ")),
                                sg.Combo(input_devices, key="sg_input_device", default_value=data.get("sg_input_device", "")),
                            ],
                            [
                                sg.Text(i18n("output device       ")),
                                sg.Combo(output_devices, key="sg_output_device", default_value=data.get("sg_output_device", "")),
                            ],
                        ], title=i18n("Audio settings (use same device driver; MME works best)"),
                    )
                ],
                [
                    sg.Button(i18n("start"), key="start_vc"), sg.Button(i18n("stop"), key="stop_vc"),
                    sg.Text(i18n("Latency (ms): ")), sg.Text("Stopped or disconnected", key="infer_time"),
                ],
            ]
            self.window = sg.Window("Audio client", layout=layout)
            self.event_handler()
            self.window["infer_time"].update(f"Connected to {server_address}")
            print(f"Connected to audio server at {server_address}")
        def event_handler(self):
            while True:
                event, values = self.window.read()
                if event == sg.WINDOW_CLOSED:
                    self.flag_vc = False
                    exit()
                if event == "start_vc" and self.flag_vc == False:
                    if self.set_values(values) == True:
                        self.start_vc()
                        settings = {"sg_input_device": values["sg_input_device"], "sg_output_device": values["sg_output_device"]}
                        with open("values_sender.json", "w") as j:
                            json.dump(settings, j)
                if event == "stop_vc" and self.flag_vc == True:
                    self.flag_vc = False
                    time.sleep(1)
                    self.window["infer_time"].update("Stopped or disconnected")
        def set_values(self, values):
            self.set_devices(values["sg_input_device"], values["sg_output_device"])
            return True
        def start_vc(self):
            self.flag_vc = True
            self.config.samplerate=40000
            print("Voice capture started")
            thread_vc = threading.Thread(target=self.soundinput)
            thread_vc.start()
        def soundinput(self):
            # accept audio input
            received_data = b""  # Initialize an empty bytes object to accumulate data
            with sd.InputStream(channels=1, callback=self.audio_callback, blocksize=1024, samplerate=self.config.samplerate, dtype="float32"):
                while self.flag_vc:
                    time.sleep(3)
            print("Voice capture stopped")
            time.sleep(1)
            self.window["infer_time"].update("Stopped or disconnected")
        def audio_callback(self, indata, frames, times, status):
            # audio processing
            start_time = time.perf_counter()
            print(f'{len(indata.tobytes())}, {len(indata)}')
            try:
                client_socket.sendall(indata.tobytes())
            except Exception as e:
                print(f"Error sending audio data: {e}", file=sys.stderr)
            """try:
                data = sock.recv(frames * channels * 4)  # 4 bytes per float32 sample
                if len(data) > 0:
                    outdata = np.frombuffer(data, dtype=np.float32)
                    sd.play(outdata, samplerate=self.config.sample_rate, channels=1)
            except Exception as e:
                print(f"Error receiving audio data: {e}", file=sys.stderr)"""
            #outdata[:] = indata
            total_time = time.perf_counter() - start_time
            self.window["infer_time"].update(int(total_time * 1000))
        def get_devices(self, update: bool = True):
            # Get device list
            if update:
                sd._terminate()
                sd._initialize()
            devices = sd.query_devices()
            hostapis = sd.query_hostapis()
            for hostapi in hostapis:
                for device_idx in hostapi["devices"]:
                    devices[device_idx]["hostapi_name"] = hostapi["name"]
            input_devices = [
                f"{d['name']} ({d['hostapi_name']})"
                for d in devices
                if d["max_input_channels"] > 0
            ]
            output_devices = [
                f"{d['name']} ({d['hostapi_name']})"
                for d in devices
                if d["max_output_channels"] > 0
            ]
            input_devices_indices = [
                d["index"] if "index" in d else d["name"]
                for d in devices
                if d["max_input_channels"] > 0
            ]
            output_devices_indices = [
                d["index"] if "index" in d else d["name"]
                for d in devices
                if d["max_output_channels"] > 0
            ]
            return (input_devices, output_devices, input_devices_indices, output_devices_indices)
        def set_devices(self, input_device, output_device):
            # set output device
            (input_devices, output_devices, input_device_indices, output_device_indices) = self.get_devices()
            sd.default.device[0] = input_device_indices[input_devices.index(input_device)]
            sd.default.device[1] = output_device_indices[output_devices.index(output_device)]
            print(f"Using input device: ({str(sd.default.device[0])}) : '{str(input_device)}'")
            print(f"Using output device: ({str(sd.default.device[1])}) : '{str(output_device)}'")
    gui = GUI()
