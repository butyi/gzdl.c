# gzdl.c

Linux CLI Downloader for Motorola/Freescale/NXP MC68HC908GZ60

## Terminology

### Monitor loader

Monitor loader is a PC side software which can update program memory of an empty (virgin) microcontroller.
Disadvantages are it needs special hardware interface and slow. 
I have already written such a software for GZ family. [See here.](https://github.com/butyi/gzml/)

### Bootloader

Bootloader is embedded side software in the microcontroller,
which can receive data from once a hardware interface and write data into own program memory. 
This software needs to be downloaded into microcontroller only once by a monitor loader.
[See here.](https://github.com/butyi/gzbl/)

### Downloader

Downloader is PC side software. The communication partner of Bootloader. 
It can send the pre-compiled software (or any other data) to data to microcontroller through the supported hardware interface. 

## What is this?

This is the Downloader for GZ series microcontroller.

## Hardware

To be HW interface easy and cheap, buy TTL USB-Serial interface from China.
I use FT232RL FTDI USB to TTL Serial Adapter Module for communication. This is supported by both Linux and Windows 10.
With this you do not need voltage level converter (MAX232) on microcontroller side hardware, you just need some cable and that's it.

More details about hardware is [here.](https://github.com/butyi/gzml/)

## How get use gzdl?

The keyword is Linux. The software was developed on my Ubuntu Linux 16.04 LTS. 
- Open terminal
- First check out files into a folder.
- Compile gzbl by `gcc gzbl.c -o gzbl`

## How to use gzdl?
`./gzdl`

Print out help about version, command line options, tipical usages. 

`./gzdl -t -b 57600`

Open a serial terminal with defined baud rate.

`./gzdl -b 57600 -f ~/prg.s19 -c -m`

Download file prg.s19 from home folder with defined baud rate.

`./gzdl -t -b 57600 -f ~/prg.s19 -c -m`

Download file prg.s19 from home folder with defined baud rate and remains in terminal mode.

## Further Development

If you only improve PC side features, just 
- Edit gzdl.c 
- Compile gzdl by `gcc gzdl.c -o gzdl`
- Enjoy it.

## Baud rate

Baus rate is depends on the bootloader. Download is only possible if defined 
baud rate is same as implemented in the bootloader.
Refer bootloader code [here](https://github.com/butyi/gzbl/).

## Supported interfaces

- Motherboard mounted ttyS0
- USB Serial interface ttyUSB0 
  ( [FT231XS](https://www.ftdichip.com/Support/Documents/DataSheets/Cables/DS_Chipi-X.pdf) and
    [ATEN UC232A1](https://www.aten.com/global/en/products/usb-&-thunderbolt/usb-converters/uc232a1/) ) 

## License

This is free. You can do anything you want with it.
While I am using Linux, I got so many support from free projects, I am happy if I can help for the community.

### Keywords

Motorola, Freescale, NXP, MC68HC908GZ60, 68HC908GZ60, HC908GZ60, MC908GZ60, 908GZ60, HC908GZ48, HC908GZ32, HC908GZ16, HC908GZ, 908GZ

###### 2019 Janos Bencsik



