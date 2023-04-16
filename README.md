# esp8266-mqtt-display [![Build Status](https://github.com/probonopd/esp8266-mqtt-display/actions/workflows/compile.yml/badge.svg)](https://github.com/probonopd/esp8266-mqtt-display/actions/workflows/compile.yml)

The display will show messages sent over MQTT. When the button is pressed, a timestamp will be published over MQTT to a device specific channel.

The sketch is intended to be robust against intermittent WLAN. It tries to reconnect to both WLAN and MQTT if the connection is lost.

## System Requirements

* ESP8266 device like NodeMCU
* 16x2 character LCD with i2s backpack

## Downloading

From GitHub Actions under "Summary", "Artifacts"

## Flashing

E.g., on FreeBSD:

```
python3 -m pip install esptool
sudo -E python3 -m esptool erase_flash
sudo -E python3 -m esptool --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB  0x00000 firmware.bin && sudo screen /dev/ttyU0 115200
```

__NOTE:__ On FreeBSD, flashing with CH340 is [not working](https://github.com/espressif/esptool/issues/272). Use, e.g., a Prolific USB to Serial adapter instead.

## Pinout

| WeMos NodeMCU ESP8266 | Periperhal            |
|-----------------------|-----------------------|
| D0                    | Button (built in)     |
| D1                    | Display SCL           |
| D2                    | Display SDA           |
| VIN                   | Display VCC           |
| GND                   | Display GND           |
| D4                    | i2s DAC LRC           |
| D8                    | i2s DAC BCLK          |
| RX                    | i2s DAC DIN           |
| GND                   | i2s DAC GND           |
| VIN (3V3 = lower vol) | i2s DAC VCC           |

## Configuring

Configuration is done via the Access Point captive portal that appears if the device cannot connect to a known WLAN network within 60 seconds.

## Debugging

Serial:

```
screen /dev/ttyU0 115200
```

## Ideas for future features

Contributions are welcome.

- [ ] Use a speaker or DAC to play messages
- [ ] ...
