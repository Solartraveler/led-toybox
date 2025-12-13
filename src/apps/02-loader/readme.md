Implements DFU

Like the loader from ST, but its loading the progam to the RAM and then runs it.

Intended to be flashed for fast testing of firmware changes.

This project is a modified copy of
[03-loader](https://github.com/Solartraveler/UniversalboxArm/tree/a2017e5d17217898de555d77526e7ae967745359/src/apps/03-loader)

It has been adjusted to the STM32F405, the display option, key input and the option to load and store programs in an external memory have been removed.