# Ossimat

This project can be used to connect a DDR (dance dance revolution)/dancing stage (as the games were used to be called here) dancing mat meant for the GameCube or Wii to a Playstation 1 or 2 via a Raspberry Pi Pico.

## Building the hardware

A female GameCube controller port and a male Playstation 1/2 controller port are necessary. The easiest way to get both is by buying an extension cord and to cut it in half. For the latter a controller cable could be used as well (they even seem to be still selling loose replacement ones).

|Pico Pin|Description|
|-----|--------|
|GPIO0|GC controller data|
|3v3|GC controller power|
|GND (again)| Both GC controller grounds and shielding if the cable you're using is fancy enough|
|GPIO6|PS controller select (also known as ATT)|
|GPIO7|PS controller clock (clk)|
|GPIO8|PS controller cmd (data coming from the Playstation)|
|GPIO9|PS controller ack|
|GPIO10|PS controller data (data going to the Playstation) |
|VSYS|PS controller (3 V) Vcc|
|GND|PS controller ground|

Additionally a pullup resistor is necessary between the GC controller data line (GPIO0) and the 3.3 V output of the Pico (3v3). I tried using the internal pullup, but that caused some issues with third party controllers and this fixed the issues.

* Playstation controller port pinout: https://pinoutguide.com/Game/playstation_9_pinout.shtml
* GameCube controller port pinout http://www.int03.co.uk/crema/hardware/gamecube/gc-control.html

## Building the software

Assuming you the `PICO_SDK_PATH` environment variable set to the SDK.

```
mkdir build
cd build
cmake  ..
```

## Questioning of the existance of this project

### Why not just get a compatible dancing mat?

I picked up a Wii dancing mat for cheap, but I didn't like the song selection of the Wii games (also based on the cover they look pretty sterile).

### Why not use an existing adapter as the dancing pads register on both consoles as just a normal controller?

I didn't realise until very late that any normal controller converter would probably work.

### Why not modify existing projects or libraries which deal with the controllers

Because it's more fun this way.

### So this is basically just a GC to PS1/2 controller adapter which only maps the D-pad, A and B?

Yes.

## Planned features

If I'll continue with this project it'll probably turn into an absolute kitchen sink, but here are some things planned:

### USB mouse to Playstation mouse

I got Rollercoaster Tycoon for PSX and the official mice are pretty hard to find and expensive as well, while most of us have atleast a handful USB mice lying around somewhere.

### USB keyboard to GameCube keyboard

You'd probably 

## The name?

Wink if you get it.

## References

Additionally