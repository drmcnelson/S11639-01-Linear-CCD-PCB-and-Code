# S11639-01-Linear-CCD-PCB-and-Code
PCB, firmware and host software for the Hamamatsu S11639-01 Linear CCD with 16 bit ADC and SPI interface, intended for Teensy 4, Arduino, and Raspberry family devices.  The Hamatsus S11639-01 is spec'd at 0.5mV dark noise, compared to 5mV for the Toshiba TC1304.  Response is similar to the Toshiba and Sony devices, but the dynamic range is larger at 8,000 (compared to approximately 300), largely due to the reduced noise and dark level.  A low noise high precision front end and 16bit ADC is appropriate for the Hamamatsu.

This repository provides electrical design files for the Hamamatus S11639-01 linear CCD sensor board as described, plus a carrier for the Teensy 4.0 that cables to the sensor board with a 20 pin ribbon cable.  carrier and firmware in c++ and "sketch" source codes.  The firmware uses the standard Arduino libraries plus a small number of optional register level enhancements for the i.MX RT MCU, that provide faster and more constant interrupt latency and SPI transfers with less overhead .

<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/b093d3cd-5eb3-4b4a-999f-7dd358d39edb" height = "200">
<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/e8fe5499-a028-4e19-9836-888f1290f96d" height = "200">
<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/ab9136c1-5f4f-4293-9427-17702c0a4946" height = "240">
<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/1475500b-c9e5-46a4-a24e-52f69d108edd" height = "240">

The sensor board hosts a socketed  Hamamatsu S11639-01 linear CCD with a high precision low noise differential front end interfaced to a differential 16bit 1MSPS ADC.  SPI and logic signals are brought to a double row ribbon compatible connector.  The chip level logic signals are translated the logic level presented to the external side of the ADC.  Power is 5V for the sensor and instrumentation circuit and from 1.7V to 5.5V for the external digital interfaces. The onboard reference voltage is 4.096V with a header for an optional user supplied reference voltage.  An analog header allows the user to monitor the direct signal from the sensor or bypass the internal ADC for an external digitizer.

The control board hosts a Teensy 4 with a double row ribbon compatible connector to interface to the sensor board, a single row header with extra pins for trigger input and sync and busy ouputs, further pins for analog input and digital I/O, and a 5V power supply output for the sensor board.  Digital power (VDD) is supplied to the sensor over the ribbon cable connector.

The firmware implements a state engine to control and read data from the S11639-01.  The controller appears as a serial port or com port with human readable command, including a help command that produces a listing of all of the available commands.  The device can collect clocked frames or sets of frames, triggered frames or triggered sets of clocked frames (triggered kinetic series), and gated frames.  Trigger input, and busy and sync output pins support synchronization with other devices.   A Python utility and class library and a C language utility and library provide two options for working with the device and integrating into equipment and experiments 

The following shows a spectrometer built with the S11639-0 and a fluorescent lamp spectrum collected through an optical fiber and 200um slit with the fluorescent ceiling lamp at a distance of about 2 meters from the end of the fiber.

<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/1929ddd1-9707-4e3b-8688-1dc88f1adc97" height = "240">
<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/8cacd840-bc7f-4735-871c-a12a61370125" height = "240">
<img src="https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/43aa3c30-d34e-46f5-930a-90824b382119" height = "400">

