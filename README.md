# picovox
> Current state: *pre-alpha*

## What is it?
Picovox is a low-cost device that serves as an *LPT audio card for old computers* (primarily DOS).
Picovox is able to emulate (at least partially) seven audio devices: *Covox*, *Stereo-On-1*, *FTL Sound Adapter*, *Disney Sound Source*, *OPL2LPT*, *TNDLPT* and *CMSLPT*.
For more information and current progress, check out: [Dedicated vogons thread](https://www.vogons.org/viewtopic.php?t=109086)

<details>
<summary>Current state of emulation</summary>

| Device | Description |
|-----|-----------|
|Covox|✅ Almost perfect, 96 kHz sampling rate.|
|FTL Sound Adapter|✅ Same as Covox with proper detection.|
|Stereo-On-1|✅ Very close to Covox, with stereo and proper detection|
|Disney Sound Source|❓ Not that great, but detection should be flawless.|
|OPL2LPT|🆗 Close enough to original AdLib, 48 kHz sampling rate.|
|TNDLPT|❓ Sound is very close to the original, detection failing.|
|CMSLPT|❌ Experimental support, produces some sound.|

</details>

## Why?
I wanted to make a device similar to [PicoGUS](https://picog.us/) for my laptop (without ISA slot), especially since the original devices are a bit pricey and I am a poor student.
As a matter of fact, not only is this project of passion, I also use it as my thesis at university.

### Why does the code look the way it does?
I know that it is probably not the best code you have ever seen. Pardon me, it's my first bigger project and I am working on it on my own. Slowly, I will refactor some parts of it to look better and be more readable to all of you.

## How can I build one?
As of now, we are still in pre-alpha. 
Sadly, I cannot provide any official building instructions. 
I strongly discourage you from building your own prototype.

<details>
<summary>Click here for pre-alpha unofficial building guide</summary>

> **⚠️ CAUTION**
> 
> Design WILL change soon, therefore I discourage you again from building it. Official version will be provided with a detailed guide and with PCB design you can use! If you don't take my advice seriously, here you go:

Basically, all you need is a Raspberry Pi Pico 2, PCM5102 and something to connect your Pico safely to the LPT port (I suggest using some LLCs). A simple button is also handy.
Only passive voltage drop will not be sufficient since three pins of the LPT port used by picovox are input pins. 
3.3V might be enough for your computer, but I did not test it.

The only thing you need to do is to connect pins of the LPT and the PCM5102 to the Pico in the correct order.
Don't forget to convert 5V of the LPT port to 3.3V of Pico, otherwise you risk something horrible I have not tested.

The correct pinout is written in *config.h*. 
Beware that you can change the pins (as long as you keep D0-D7 in the same order, from the lowest to the highest, and place Strobe next to Data pins), 
but keep in mind that you also **need to change the definitions in each PIO file!** *(Yes, it is planned to be generated automatically.)*
Also, you can connect a button for switching modes. **Connect the button to the ground, not 3.3V!** You do not need any capacitor, the software debounce is implemented.

Also, don't forget to solder pads to your PCM5102 module in the following configuration: 1L, 2L, 3H, 4L

If your device does not work, I have no clue why. It can be due to bad hardware, design, software, anything. I am really sorry that you wasted your time.

</details>

## How do I use it?
In case you steal my only prototype (or you build your own, even though I discouraged you), you simply plug the device into your laptop/computer, power up your Pico, plug in your favourite speakers and everything should work.
In case of Covox/FTL/DSS/Stereo-on-1 you can use them right out of the box. In case of OPL2LPT, TNDLPT and CMSLPT you can use them in the same way as the listed devices; either with games with native support, or load their drivers/patches.
~You can switch between modes via provided program.~ Not yet. But you can use that handy button to switch between modes yourself. 
~If you want to check current mode, you can simply use the program.~ Again, not yet. You can try to guess, scroll through all the modes until it works or you can connect the Pico to your PC and via serial console check currently loading device.

> [!CAUTION]
> Don't blow your ears off! Since volume is not standardized between devices, be cautious. 

## Progress and future
Right now, we are in pre-alpha state. However we are slowly but surely approaching *alpha 1* with following milestones:

- Support for seven LPT audio devices. ✅
- Correct detection of them. 🔜
- PCB design you can use. ⌛
- Support for software control (switching modes etc.) ❌

As the name *alpha 1* implies, there may be more alpha versions. I would like to finalize the device to generally usable state by the end of the alpha phase. Beta will focus mostly on testing and potentially adding more features.

**Picovox is basically Pico 2 connected to the LPT port. It is not a coincidence I design the PCB with bidirectional data pins in mind. This device might also serve as a printer and/or storage emulator in the future.**

## Licensing
All the code provided in this repository is licensed under *GPL-3.0-or-later*. 
Some parts are dual-licensed (e.g. the square library under BSD-3 clause and the emu8950 under MIT licence, everything is explicitly stated). 
*Also, if you happen to meet me by accident and this project seems at least a bit useful to you, you can buy me a beer. Or a soda. Or a chocolate bar.
It's entirely up to you — I won't be mad at any present, you know?*

## Thanks
Thank you all for your support. Not only have you visited this page, you have also read all the way down there! (Unless you skipped right at the end and missed potentially useful information.) 
Still, there are some people and projects I would like to thank explicitly:

- [Ian Scott](@polpo) and the whole team around PicoGUS for inspiring me and also for helping me with OPL2 emulation.
- [Jarno Lehtinen](https://github.com/mcgurk/Covox) for his McDSS, pretty similar device built on top of ESP32.
- [The Raspberry Pi](@raspberrypi) for providing not only excellent piece of hardware, but also great C SDK together with tons of libraries (also pico-extras library).
- [Graham Sanderson](https://github.com/kilograham/rp2040-doom) (OPL2) and [Aaron Giles](https://aarongiles.com/) (TND, CMS) for providing sound emulation libraries to PicoGUS (that I could use).
- [Vogons community](https://www.vogons.org/) for a place to share my ideas and to find people with similar interests.
- [Serdaco](https://www.serdaco.com/default.aspx) for distribution of the OPL2LPT, CMSLPT, TNDLPT and many other devices. Please, in case you prefer real sound over emulated, these are the devices you are probably looking for.

- *My family, my friends and everyone who has supported me any way. Without you I would not have done it.*
