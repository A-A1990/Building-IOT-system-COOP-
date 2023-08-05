
import collections
import os
import signal
import socket
import threading
import time

import busio
from board import SCL, SDA

from digi import xbee
from digi.xbee.devices import XBeeDevice, ZigBeeDevice
from digi.xbee.models.status import NetworkDiscoveryStatus
from oled_text import OledText, Layout64



PORT = "/dev/ttyUSB0"

BAUD_RATE = 9600
i2c = busio.I2C(SCL, SDA)

oled = OledText(i2c, 128, 64)

Temp = 24
# 0013A2004191AC43
print(" +------------------------------------------------+\n")
received_dataWhole = ""
received_dataSplit = ""

device = ZigBeeDevice(PORT, BAUD_RATE)

check_devices = {}
available_devices = []
number_of_devices = 0
device_who_send_data = device

socket_jop = collections.deque([])


class DeviceInNetwork:
    def __init__(self, name, ip64, ip16):
        self.node_name = name
        self.ip64 = ip64
        self.ip16 = ip16

    def __str__(self):
        return f"{str(self.node_name)}({str(self.ip64)}){str(self.ip16)}"


def socket_op():
    s = socket.socket()
    #127.0.0.1
    s.bind(('127.0.0.1', 7588))
    s.listen(1)
    java_client, _ = s.accept()
    while True:
        if socket_jop:
            java_client.send(socket_jop.pop().encode())


def main():
    threading.Thread(target=socket_op).start()
    oled.clear()
    print(" +------------------------------------------------+")
    print(" | XBee Python Library Send Broadcast Data Sample |")
    print(" +------------------------------------------------+\n")
    oled.layout = Layout64.layout_5small()
    try:
        init()
        oled.text("Recived", 1)
        SpaceFor = len(str(Temp)) * " "
        oled.text("T " + SpaceFor + ",RH" + SpaceFor, 2)
        oled.text("Send", 4)
        oled.show()

        while True:
            print("DDD")
           
            if finish_scanning_network:
                print("Who do you want to send?")
                try:
                    read = int(input())
                    for i in range(len(available_devices)):
                        if i == read:
                            print("What do you want to send?")
                            data = input()

                            device.send_data_64_16(available_devices[i].ip64, available_devices[i].ip16, data)
                            oled.text(data, 5)
                except ValueError:
                    print("Please enter a number.")

    finally:
        print("Fin")
        process_interrupted()


def data_receive_callback(xbee_message):
    if finish_scanning_network:  # == True
        def pairs(iterable):
            for i in range(0, len(iterable), 2):
                yield iterable[i:i + 2]

        received_data_whole = xbee_message.data.decode()
        data = received_data_whole.split("-")
        sensors_data = data[0]
        sender_ip = xbee_message.remote_device.get_64bit_addr()
        socket_jop.append(time.strftime(str(sender_ip) + "," + "%H:%M:%S",
                                        time.localtime()) + "," + sensors_data)

        for key, value in pairs(sensors_data.split(",")):
            if key == "T":
                temp = ("{0}:{1}, ".format(key, value))
            elif key == "RH":
                rh = ("{0}:{1}, ".format(key, value))

        for i in range(len(available_devices)):
            if sender_ip == available_devices[i].ip64:  
                oled.text(str(available_devices[i].ip16) + ": " + str(temp) + str(rh), i + 2)


def init():
    device.open()
    xbee_network = device.get_network()
    device.add_data_received_callback(data_receive_callback)
    xbee_network.set_discovery_timeout(15)
    xbee_network.clear()
    xbee_network.add_device_discovered_callback(callback_device_discovered)
    xbee_network.add_discovery_process_finished_callback(callback_discovery_finished)
    xbee_network.start_discovery_process()


def callback_device_discovered(remote):
    global number_of_devices
    single_device = DeviceInNetwork(remote.get_node_id(), remote.get_64bit_addr(), remote.get_16bit_addr())

    available_devices.append(single_device)
    number_of_devices = number_of_devices + 1


def callback_discovery_finished(status):
    global finish_scanning_network

    if status == NetworkDiscoveryStatus.SUCCESS:
        print("Discovery process finished successfully.")
        finish_scanning_network = True
    else:
        print("There was an error discovering devices: %s" % status.description)
    for i in range(len(available_devices)):
        print("D", i, ": ", available_devices[i].ip64, " , ", available_devices[i].ip16)


def process_interrupted(signum=None, frame=None):
    if device is not None and device.is_open():
        device.close()

    os.system('kill -STOP %d' % os.getpid())


if __name__ == '__main__':
    signal.signal(signal.SIGTSTP, process_interrupted)
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    main()
    #threading
