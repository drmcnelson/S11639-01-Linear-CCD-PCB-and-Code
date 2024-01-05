# S11639-01-Linear-CCD-PCB-and-Code
PCB and Code for the Hamamatsu S11639-01 Linear CCD with 16 bit ADC and SPI interface.  The system can be used with Arduino, Teensy or Raspberry Pi that have sufficient number of digital lines and sufficient performace for interrupts and SPI.

The directories have KiCAD files for the sensor board, and for a carrier for a Teensy 4.0 with connectors matching the sensor board.  The envisioned interconnect is a short (e.g. 2") 20 pin ribbon cable for logic signals and $\mathrm V_D$ and a two wire cable for 5V power.

Caveat: WORK IN PROGRESS.  Files were recently seen for FAB.

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
The following timing diagrams appear on page 5 and 6. The integration interval is described as equal to the time during which the ST pin is high (start pulse width) plus another 48 clock cycles.  Output appears on the video pin and is to be sampled at each rising edge asserted by the sensor on the Trig pin.  The rising edge asserted on the EOL pin signals the end of the record.  This convenient for processing. The trig signal from the sensor can trigger conversion in the ADC and with an appropriate delay initiate an SPI transfer from the ADC to the host, and the EOL can initiate transfer or further processing of the completed record.  So to take advantage of these features we want to bring the Trig and EOS pins to the connector.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/309ec305-8dee-475f-9f3f-8bd6a03be575)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2908a0fc-5c88-4da0-84a4-8e4f050bc7ad)

### Design files
The following shows the layout for the board.   The header in the lower left sets Vref and the supply voltage for the analog section, pins 2-3 should be jumpered to use the internal reference.  The trim pot adjust the offset level in the analog circuit, there are two test points to the right of the trim pot, and the three pin header in the middle duplicates the analog signals delivered to the ADC. The header across the top of the board presents the four lines from the sensor, CLK, ST, TRIG and EOS and the four SPI lines from the ADC. The VD pins on this header power the external side of the logic level converters and the SPI side of the ADC.   The small jumper below this header connects the ADC CNVST input to the sensor TRIG line or routs CNVST to the host. 

![Screenshot from 2024-01-04 10-20-41](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/28649053-fdd7-4f8d-8f97-63fbb6c821ae)

The sensor is mounted to the reverse side of the board as shown in the following.

![Screenshot from 2024-01-04 10-23-30](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/04e5b704-4752-414b-a01d-ac059fea0315)

A second design is provided for a carrier for a Teensy 4.0, with matching connectors for the signals and power.  The interconnect is envisioned as a short (e.g. 1" or 2") 20 pin ribbon cable and a 2 wire cable for 5V power.

![Screenshot from 2024-01-02 21-02-08 p500](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/b8af760a-8539-4766-ba1b-6273e601c9d3)






