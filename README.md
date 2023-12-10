ESP32-based Advanced Smart Meat Thermometer

Features:

- Two 2.5mm sockets to fit two separate standard BBQ thermistor-based probes
- Steinhart-Hart calibration, based on immersing probes & DS18B20 digital sensor in boiling water, obtaining readings at 75, 50 and 25C.  Will automate later.
- Web interface that does not require an app - TODO: Web interface only updates while loaded.  Want graph data to persist even when not viewing webpage.  Also needs sound alarm function on webpage.
- 16-bit ADS1115 ADC for precise measurement
- Hand balanced voltage divider resistors for both probe inputs
- Speaker hooked up to DAC to play 10s long, 8bit, 8khz wav audio
- Web interface for flashing new firmware/web page

To compile:

1. Use OLD Arduino IDE (Arduino 2 IDE does not have ESP32 sketch data uploader tool)
2. Install ESP32 sketch data uploader tool into Arduino IDE
3. After selecting ESP32 board and preparing to flash, select Tools, Partition Scheme, Minimal SPIFFS (1.9MB/190KB) - THIS IS THE ONLY WAY TO FIT johnnycash.h in memory
4. Upload firmware to ESP32
5. Use Tools, ESP32 Sketch Data Upload to upload html files in /data folder to ESP32.

If you would like to use web interface to upload new html files, use the same ESP32 Sketch Data Upload tool while board is disconnected, it will generate .bin file and fail to upload.  
Identify directory containing .bin file from error message, use web uploader, select "Filesystem" instead of Firmware, flash this new generated .bin file.  
