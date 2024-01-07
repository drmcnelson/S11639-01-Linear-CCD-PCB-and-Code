# S11639-01-Linear-CCD-PCB-and-Code
PCB, firmware and host software for the Hamamatsu S11639-01 Linear CCD with 16 bit ADC and SPI interface, intended for Arduino, Teensy and Raspberry family devices that have sufficient performance for interrupts and data transfers.

The directories have KiCAD files for the sensor board, and for a carrier for a Teensy 4.0 with connectors matching the sensor board.  The envisioned interconnect is a short (e.g. 2") 20 pin ribbon cable for logic signals and $\mathrm V_D$ and a two wire cable for 5V power.

Caveat: WORK IN PROGRESS.  Files were recently sent for FAB.

## Introduction
The Hamamatsu S11639-01 is a highly sensitive linear CCD with a [quantum efficiency of about 0.8](https://ibsen.com/wp-content/uploads/Tech-Note-Quantum-efficiency-conversion-note-1.pdf) and a dynamic range of 10,000. Notabbly the dark signal level is 0.2mV and conversion efficiency is listed as 20 $\mu \mathrm{V/e^-}$ .  This puts the S11639-01 sensor in the ballpack with some of the high end CCD systems sold to research laboratories in terms of performance.  To capture that performance as digitial data, we use a low noise front end design with a 16 bit differential input ADC.

Along with the circuit design for the sensor board, we provide KiCAD files for a matching carrier for a Teensy 4.0.  We choose this microconroller from other boards in the Arduino and Raspberry families, because of its high speed USB.  Aquiring the frame data from the sensor into the micontrocontroller generally takes about 2.5ms/frame with a 30MHz to 50 MHZ SPI. Transferring data from the microcontroller to its host (generally a desktop PC) over high-speed USB takes less than 100us/frame depending on processing in the desktop.  The maximum frame rate in this architecture is then determined by the SPI interface.

## Electrical
### Signal
The [S11639-01 datasheet](https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s11639-01_kmpd1163e.pdf) provides the following electrical and optical characteristics on page 2.  Note that the clock can be from 200KHz to 10MHz, the output is positive going, and there is a large variation in output impedance, offset and saturation.   This means we have to accomodate a range of 0.3V to 3.4V with output impedance from 70 to 260 ohms (a factor of almost 4), and match the signal to the input range of our ADC.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/998c54a0-a48c-4cc0-90cc-10f83ad4ebfb)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2956b1a0-4789-4691-9a65-d3e1a5f0222f)

### Front-end circuit
The following approach exploits conversion to differential to provide a low noise input for a differential ADC with a gain of 2 and using only a dual opamp and single ended supply.  Because of the variation in offset and amplitude we need to set the Vref above 3.4V.  We might note that the dark signal is one part in 10,000 compared to 1 part in 65,536 for the LSB of the 16 bit ADC.

![Screenshot from 2024-01-04 13-57-02 p650](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/e52e5d7f-0f3e-48b2-8e50-fcf57cc9e338)

![CCDbufferTypCase](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/d2938130-6e73-46d5-802d-ec7fc1c31b6b)

### Timing
The following timing diagrams appear on page 5 and 6 of the S11639-0 datasheet. The integration interval is described as equal to the time during which the ST pin is high (start pulse width) plus another 48 clock cycles.  Output then appears on the video pin and for each pixel the sensor asserts TRIG to signal that the value is ready to be sampled.  This continues for the 2048 word record and the sensor then asserts EOS to signal the end of the record.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/309ec305-8dee-475f-9f3f-8bd6a03be575)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2908a0fc-5c88-4da0-84a4-8e4f050bc7ad)

#### ADC timing
For the ADC, we have the following diagrams from pages 37 and  41 of the
[MCP33131D datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/MCP33131D-Data-Sheet-DS20005947B.pdf).
The input is sampled onto two capacitors (positive and negative side) until the rising edge of CNVST.  The signal is then locked and the ADC proceeds with conversion.  The conversion is completed in 700ns and the data word is available for transfer over SPI.  This of course, has to be repeated until the 2048 word sensor frame is completed.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/21d1a17f-d1fa-41a8-bd21-e409523d1ecc)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/24a460e8-97a7-4456-9b05-4b521795b1f2)

#### Integration
The sensor's TRIG and EOS signal provide a convenience in that data word acquisition can be tied to the TRIG signal and frame level processing tied to the EOS signal.  in our design we connect the trig signal from the sensor to the CNVST input of the ADC and to a digital input pin on the host which we connect to an interrupt to trigger the SPI transfer after a 700ns delay.  The EOS is connected to another pin on the host and triggers frame level processing, e.g. transferring completed frames from the microntroller to its desktop host.

### Design files
The following shows the layout for the board.   The header in the lower left sets Vref and the supply voltage for the analog section, pins 2-3 should be jumpered to use the internal reference.  The trim pot adjust the offset level in the analog circuit, there are two test points to the right of the trim pot, and the three pin header in the middle duplicates the analog signals delivered to the ADC. The header across the top of the board presents the four lines from the sensor, CLK, ST, TRIG and EOS and the four SPI lines from the ADC. The VD pins on this header power the external side of the logic level converters and the SPI side of the ADC.   The small jumper below this header connects the ADC CNVST input to the sensor TRIG line or routs CNVST to the host. 

![Screenshot from 2024-01-04 10-20-41](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/28649053-fdd7-4f8d-8f97-63fbb6c821ae)

The sensor is mounted to the reverse side of the board as shown in the following.

![Screenshot from 2024-01-04 10-23-30](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/04e5b704-4752-414b-a01d-ac059fea0315)

A second design is provided for a carrier for a Teensy 4.0, with matching connectors for the signals and power.  The interconnect is envisioned as a short (e.g. 1" or 2") 20 pin ribbon cable and a 2 wire cable for 5V power.

![Screenshot from 2024-01-02 21-02-08 p500](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/b8af760a-8539-4766-ba1b-6273e601c9d3)






