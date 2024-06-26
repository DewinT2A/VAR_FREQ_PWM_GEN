/*
 * PWM_GENERATOR for arduino mega/2560
 * 
 * Connect to UART with BAUD = 250000, or change it below
 *
 * To set PWM frequency, write in terminal (40 Hz to 65000 Hz): "FREQ=55" or "FREQ=40436"
 * To set PWM duty cycle write in terminal (0-255): "PWM=1" or "PWM=252" 
 *
 * Default is 1 kHz with 50% duty cycle.
 *
 * As frequency gets higher, duty cycle resolution gets lower. frequencies below 7800 Hz have full 8 bit resolution
 *
 * Created: 20-06-2024
 * Author : DES
 */ 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//Clock
#define F_CPU 16000000UL	//CPU Frequency
#define UART_BAUD	250000	//UART baudrate - higher baud  rate without timing error

#define Rx_Buf_Len	50		//Length of UART receive buffer (max number of chars to be received at once)
#define Tx_Buf_Len	50		//Length of UART transmit buffer (max number of chars to be transmitter at once)
char UART_Rx [Rx_Buf_Len];	//UART receive buffer
char UART_Tx [Tx_Buf_Len];	//UART transmit buffer
char UART_RX_Data;			//UART RX data byte
uint8_t DutyCycle;			//PWM Duty Cycle

volatile char Flag_0;		//Timer 1 interrupt flag
volatile char Flag_2;		//UART0 RX complete interrupt flag


void Timer1_Init();
void Uart_Init (unsigned int ubrr);
void putchUSART (char tx);
void putsUSART (char *ptr);
void UART0RX (void);
void Uart_Check_String (void);
void Uart_Clean (void);
void SetPWM (void);
void SetFreq (uint16_t Freq);

int main(void)
{
	//Timer 1
	Timer1_Init();
	
	//UART0 = USB UART
	unsigned int Ubrr;
	Ubrr = (((F_CPU/8)/UART_BAUD)-1);		//Calculate Ubrr
	Uart_Init (Ubrr);						//Init UART0 and set baud rate
	
	//PWM 13 = output pin
	DDRB |= (1<<PB7);
	PORTB &=~ (1<<PB7);
	
	sei();
	
	putsUSART("Init done.\n");
	
    /* Replace with your application code */
    while (1) 
    {
		//Check uart0 receive
		if (Flag_2 == 1)
		{
			UART0RX ();
		}
    }
}

ISR (USART0_RX_vect)
{
	UART_RX_Data = UDR0;
	Flag_2 = 1;
}

ISR (TIMER1_COMPA_vect)
{
	PORTB |= (1<<PB7);
}

ISR (TIMER1_COMPB_vect)
{
	PORTB &=~ (1<<PB7);
}

void Timer1_Init()
{
	//set mode - fig17-2
		//CTC mode 4
	TCCR1B |= (1<<WGM12);

	//set compare match A - mode + match - fig 17-3
	OCR1A = 1999;
	
	
	//set compare match B
	OCR1B = 1000;
	DutyCycle = 127;

	//set clock frequency - fig 17-6
	TCCR1B |= (1<<CS11);	//prescaler = 8 = freq = 2 MHz
	
	//set timer compare match interrupt
	TIMSK1 |= (1<<OCIE1A) | (1<<OCIE1B);
}

void Uart_Init (unsigned int ubrr)
{
		UCSR0B |= (1<<RXEN0) | (1<<TXEN0);		//enable rx and tx for UART0
		UCSR0B |= (1<<RXCIE0);					//enable receive complete interrupt UART0
		//^^the line above should be commented out to disable receive complete interrupt
		UCSR0C |= (1<<UCSZ00) | (1<<UCSZ01);	//bit length is 8bit, no parity, 1 start bit
		
		UBRR0L = (unsigned char)(ubrr);			//baud rate set to what is defined in main/global
		UBRR0H = (unsigned char)(ubrr>>8);		//		16-bit register
		UCSR0A = (1<<U2X0);						//enable duplex for UART0

}

void putchUSART (char tx)
{
	while (!(UCSR0A & (1<<UDRE0)));		//Waits until UART0 transmit buffer is empty
		UDR0 = tx;
}

void putsUSART (char *ptr)
{
	while (*ptr)
	{
		putchUSART(*ptr);
		ptr++;
	}
}

void UART0RX (void)
{
	uint8_t Rx_Complete = 0;
	uint8_t Counter = 0;
	uint16_t Time_Out = 0;
	
	memset(UART_Rx, 0, Rx_Buf_Len);			//clear rx buffer

	while (!Rx_Complete)
	{
		//Read Uart0 RX data
		if (Flag_2 == 1)
		{
			Flag_2 = 0;
			UART_Rx[Counter] = UART_RX_Data;
			Counter++;
			Time_Out = 0;
		}
		
		//Linefeed or cartridge return check
		if (UART_Rx[Counter-1] == '\r' || UART_Rx[Counter-1] == '\n' || Counter >= Rx_Buf_Len)
		Rx_Complete = 1;
		
		//UART Rx timeout
		if (Time_Out>2000)
		Rx_Complete = 1;
		
		Time_Out++;
	}
	
	Uart_Clean();				//remove terminating characters from rx string
	
	Uart_Check_String();		//check for terminal commands
	
	Flag_2 = 0;
	
	//Echo
	if (Rx_Complete && Time_Out<2)
	{
		putsUSART(UART_Rx);
		putsUSART("\r\n");
	}
}

void Uart_Check_String (void)
{
	char Check_String [10];

	//cant use normal assignemt, so sprintf is used
	sprintf(Check_String, "PWM=");
	if(strncmp(Check_String, UART_Rx, 4) == 0)
	{
		char TempString[4];
		strncpy(TempString, UART_Rx + 4, 3);
		DutyCycle = atoi(TempString);

		SetPWM();
	}
	
	sprintf(Check_String, "FREQ=");
	if(strncmp(Check_String, UART_Rx, 5) == 0)
	{
		char TempString[6];
		uint16_t Freq;
		strncpy(TempString, UART_Rx + 5, 5);
		Freq = atoi(TempString);
		
		SetFreq(Freq);
	}
}

void Uart_Clean (void)
{
	for (int i = 0; i<Rx_Buf_Len; i++)			//clean the input (removes \r and \n,)
	{
		if (UART_Rx[i] == '\r' || UART_Rx[i] == '\n')
		{
			UART_Rx[i] = 0;
		}
	}
}

void SetPWM(void)
{
	if (DutyCycle > 254)
		DutyCycle = 254;
	
	uint32_t temp;
	temp = DutyCycle;
	temp *= OCR1A;
	temp /= 0xFF;
		
	OCR1B = temp;
	sprintf(UART_Tx, "PWM set to %u%%\n", (DutyCycle*100)/255);
	putsUSART(UART_Tx);
	sprintf(UART_Tx, "Match A\t%u\nMatch B\t%u\n", OCR1A, OCR1B);
	putsUSART(UART_Tx);
}

void SetFreq(uint16_t Freq)
{
	uint32_t temp = 2000000;
	
	if (Freq < 40)
		Freq = 40;
	
	temp = (temp/Freq)-1;
	
	OCR1A = temp;
	SetPWM();
	
	sprintf(UART_Tx, "Frequency set to %u Hz\n", Freq);
	putsUSART(UART_Tx);
}