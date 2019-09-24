README for Plate Drive Cycle Test


Create bin image of Drive Cycle .csv file:

~/mkspiffs-0.2.3-arduino-esp32-linux64/mkspiffs -c ~/toFlash/ -b 4096 -p 256 -s {partition size} ~/spiffs_image.bin


Flash bin file to ESP partition:

python /home/mpadmalayam/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 write_flash --flash_size detect {partition offset} ~/spiffs_image.bin


EX: (ESP32 with extra RAM)
~/mkspiffs-0.2.3-arduino-esp32-linux64/mkspiffs -c ~/toFlash/ -b 4096 -p 256 -s 0x100000 ~/spiffs_image.bin

python /home/mpadmalayam/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 write_flash --flash_size detect 0x210000 ~/spiffs_image.bin
