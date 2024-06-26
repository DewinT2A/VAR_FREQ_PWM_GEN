# VAR_FREQ_PWM_GEN
Variable Frequency PWM generator AVR

Made for a arduino Mega/2560, with output on D13/PB7/LED

Default frequency and dutycycle:
1000 Hz 50%

To change frequency or dutycycle connect to UART with starndard settings and 250000 baudrate

For frequency send "FREQ=xxx" on UART, frequencies can be between 40 hz and 65kHz
For PWM dutycycle send "PWM=xxx" on UART, dutycycle can be between 0 and 255