# S11639-01-Linear-CCD-PCB-and-Code
PCB and Code for the Hamamatsu S11639-01 Linear CCD with 16 bit ADC and SPI interface.  The system can be used with microcontrollers (e.g.,Teensy 4.0) that provide SPI and digital I/O pins with sufficient speed for transfers and interrupt response time.

The directories have KiCAD files for the sensor board, and for a carrier for a Teensy 4.0 with connectors matching the sensor board.  The envisioned interconnect is a short (e.g. 2") 20 pin ribbon cable for logic signals and $\mathrm V_D$ and a two wire cable for 5V power.

Caveat: WORK IN PROGRESS.  Files were recently sent for FAB.

## Introduction
The Hamamatsu S11639-01 is a highly sensitive linear CCD with a [quantum efficiency of about 0.8](https://ibsen.com/wp-content/uploads/Tech-Note-Quantum-efficiency-conversion-note-1.pdf) and a dynamic range of 10,000 with saturation voltage at 2.0V (typical) and dark at 0.2mV (typical).  Compared to low cost linear CCDs, such as the TCD1304 and IXL511, the Hamamatsu is 8 to 10 times more sensitive and the dynamic range is 30 times larger.  This puts the S11639-01 in the ballpark of some of some of the high end CCD systems sold to research laboratories.  The board described here may be a cost effective alternative at about $50 for the BOM pluse about $400 for the sensor.  Our task then is to deliver the sensitivity and dynamic range of the Hamamatsu sensor along with firmware and software support for a rich set of triggered, gated and clocked operations.

## Electrical
### Signal
The [S11639-01 datasheet](https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s11639-01_kmpd1163e.pdf) provides the following electrical and optical characteristics on page 2.  Note that the clock can be from 200KHz to 10MHz, the output is positive going, and there is a large variation in output impedance, offset and saturation.   The latter means that we have to accomodate a range of 0.3V to 3.4V with output impedance from 70 to 260 ohms (a factor of almost 4), and match the signal to the input range of our ADC.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/998c54a0-a48c-4cc0-90cc-10f83ad4ebfb)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2956b1a0-4789-4691-9a65-d3e1a5f0222f)

### Front-end circuit
The following approach exploits conversion to differential to provide low noise and a gain of 2, using only a dual opamp and single ended supply.  We combine this with a 16 bit differential input ADC.  Because of the variation in offset and amplitude we need to set the Vref above 3.4V.  While it seems like we might be giving up a bit, the dark signal is one part in 10,000 compared to 1 part in 65,536 for the LSB of the 16 bit ADC.

![Screenshot from 2024-01-04 13-57-02 p500](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/e99c93ef-fd4c-4e27-9c6f-1f5811697b57)

![CCDbufferTypCase](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/d2938130-6e73-46d5-802d-ec7fc1c31b6b)

### Timing
The following timing diagrams appear on page 5 and 6 of the S11639-0 datasheet. The integration interval is described as equal to the time during which the ST pin is high (start pulse width) plus another 48 clock cycles.  Output appears on the video pin and for each pixel the sensor asserts TRIG to signal that the value is ready to be sampled.  This continues for the 2048 word record and the sensor then asserts EOS to signal the end of the record.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/309ec305-8dee-475f-9f3f-8bd6a03be575)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2908a0fc-5c88-4da0-84a4-8e4f050bc7ad)

#### ADC timing
For the ADC, we have the following diagrams from pages 37 and  41 of the MCP33131D datasheet.  The input is sampled onto two capacitors (positive and negative side) until the rising edge of CNVST.  The signal is then locked and the ADC proceeds with conversion.  The conversion is completed in 700ns and the data word is available for transfer over SPI.  This of course, has to be repeated until the 2048 word sensor frame is completed.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/21d1a17f-d1fa-41a8-bd21-e409523d1ecc)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/24a460e8-97a7-4456-9b05-4b521795b1f2)

#### Integration
The sensor's TRIG and EOS signal provide a convenience in that data word acquisition can be tied to the TRIG signal and frame level processing tied to the EOS signal.  To take maximum advantage of this, we connect the trig signal from the sensor to the CNVST input of the ADC and a digital input pin on the host which we connect to an interrupt to trigger the SPI transfer after a 700ns delay.  The EOS is connected to another pin on the host and triggers frame level processing, e.g. transferring completed frames from the micontroller acting as host to the sensor board, to its host, typically a desktop system.

### Design files
The following shows the layout for the board.   The header in the lower left sets Vref and the supply voltage for the analog section, pins 2-3 should be jumpered to use the internal reference.  The trim pot adjust the offset level in the analog circuit, there are two test points to the right of the trim pot, and the three pin header in the middle duplicates the analog signals delivered to the ADC. The header across the top of the board presents the four lines from the sensor, CLK, ST, TRIG and EOS and the four SPI lines from the ADC. The VD pins on this header power the external side of the logic level converters and the SPI side of the ADC.   The small jumper below this header connects the ADC CNVST input to the sensor TRIG line or routs CNVST to the host. 

![Screenshot from 2024-01-04 10-20-41](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/28649053-fdd7-4f8d-8f97-63fbb6c821ae)

The sensor is mounted to the reverse side of the board as shown in the following.

![Screenshot from 2024-01-04 10-23-30](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/04e5b704-4752-414b-a01d-ac059fea0315)

A second design is provided for a carrier for a Teensy 4.0, with matching connectors for the signals and power.  The interconnect is envisioned as a short (e.g. 1" or 2") 20 pin ribbon cable and a 2 wire cable for 5V power.

![Screenshot from 2024-01-02 21-02-08 p500](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/b8af760a-8539-4766-ba1b-6273e601c9d3)






