# S11639-01-Linear-CCD-PCB-and-Code
PCB and Code for the Hamamatsu S11639-01 Linear CCD with 16 bit ADC and SPI interface.  The system can be used with Arduino, Teensy or Raspberry Pi that have sufficient number of digital lines and sufficient performace for interrupts and SPI.

WORK IN PROGRESS - Stay tuned, uploads for the electrical design files are pending.

## Introduction
The Hamamatsu S11639-01 is a highly sensitive linear CCD with a quantum efficiency of about 0.8 and a dynamic range of 10,000 with saturation voltage at 2.0V (typical) with a 0.2mV dark signal (typical).  Compared to low cost linear CCDs offered by Toshiba and Sony, the Hamamatsu is 8 to 10 times more sensitive and the dynamic range is 30 times larger.  This puts the Hamamatsu sensor in the ballpark of some of some of the high end CCD systems sold to research laboratories, sans cooling.  At a cost of about $400 plus the board described here with a $50 BOM, the Hamamatsu can be a cost effective alternative.  Our task then is to deliver the sensitivity and dynamic range of the Hamamatsu sensor support for a rich set of triggered, gated and clocked operations.

## Signal
The [S11639-01 datasheet](https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s11639-01_kmpd1163e.pdf) provides the following electrical and optical characteristics on page 2.  Note that the clock can be from 200KHz to 10MHz, the output is positive going, and there is a large variation in output impedance, offset and saturation.   The latter means that we have to accomodate signals with a range between 0.3V to 2.3V and 0.9V to 3.4V with output impedance from 70 to 260 ohms (a factor of almost 4), and match the signal to the input range of our 16bit ADC.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/998c54a0-a48c-4cc0-90cc-10f83ad4ebfb)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2956b1a0-4789-4691-9a65-d3e1a5f0222f)

The following approach exploits conversion to differential for a gain of 2, which gives us a low noise input for a 16 bit ADC and requires only a dual opamp and single ended supply.  For a typical offset we are giving up a little less than one bit.

![Screenshot from 2024-01-04 13-57-02 p500](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/e99c93ef-fd4c-4e27-9c6f-1f5811697b57)

![CCDbufferTypCase](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/d2938130-6e73-46d5-802d-ec7fc1c31b6b)

## Timing
The following timing diagrams appear on page 5 and 6. The integration interval is described as equal to the time during which the ST pin is high (start pulse width) plus another 48 clock cycles.  Output appears on the video pin and is to be sampled at each rising edge asserted by the sensor on the Trig pin.  The rising edge asserted on the EOL pin signals the end of the record.  This convenient for processing. The trig signal from the sensor can trigger conversion in the ADC and with an appropriate delay initiate an SPI transfer from the ADC to the host, and the EOL can initiate transfer or further processing of the completed record.  So to take advantage of these features we want to bring the Trig and EOS pins to the connector.

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/309ec305-8dee-475f-9f3f-8bd6a03be575)

![image](https://github.com/drmcnelson/S11639-01-Linear-CCD-PCB-and-Code/assets/38619857/2908a0fc-5c88-4da0-84a4-8e4f050bc7ad)

## Electrical design files

