# OpenBSD-gpio-idlemon
Blinks an LED based on CPU load. When the system is idle, the LED will blink every few seconds. When the system is busy, the LED will blink rapidly.

###PC Engines ALIX
An example configuration for ALIX boards follows.

Add to /etc/rc.securelevel:

	# GPIO (all inverted, but don't use the invert bit because it is sticky)
	gpioctl -q gpio0 6 set out LED1
	gpioctl -q gpio0 25 set out LED2
	gpioctl -q gpio0 27 set out LED3

Add to /etc/rc.local:

	# signal we reached securemode
	gpioctl -q gpio0 LED1 on
	gpioctl -q gpio0 LED2 off
	gpioctl -q gpio0 LED3 on
	# as the last action, start the blinkenlights
	/usr/local/libexec/gpioidlemon /dev/gpio0 LED3

Add to /etc/rc.shutdown:

	# signal we are shutting down
	pkill gpioidlemon
	gpioctl -q gpio0 LED3 off
	gpioctl -q gpio0 LED2 off
