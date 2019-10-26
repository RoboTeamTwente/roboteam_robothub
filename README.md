# roboteam\_robothub

Hi there! Some useful things to know for using the RobotHub package and peripherals

# Getting robothub to work.

## Getting rid of ModemManager

ModemManager is a program installed on Ubuntu by default. Afaik it allows you to dial-in via a serial port. A nasty side-effect of this program is that as soon as a serial port appears it grabs it, fills it with 30 bytes of garbage, and then leaves the port unusable for 30 seconds. Since we have eduroam for our internet needs, we can get rid of this ancient piece of machinery.

`sudo apt-get purge modemmanager`

This command and a reboot should take care of it.

## User rights

On vanilla Ubuntu you can only use the serial port if you are in the dialout group, or something similar. However, by default this is not true. So you need to add yourself to the dialout group (or if that doesn't work, google a bit for a similar solution).

`sudo adduser second_user dialout`

Where `second_user` is your username. This command and a reboot should settle it.

(It also might not. Just keep googling and restarting until you find the right command 😅)
