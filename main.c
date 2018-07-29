#ifndef F_CPU
#define F_CPU   16000000UL
#endif

#include "segm.h"
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


	volatile uint8_t second;
	volatile uint8_t minute;
	volatile uint8_t hour;
	volatile uint8_t alarm_hr;
	volatile uint8_t alarm_min;
	int point;
 
/** Timer2 Interrupt (on overflow), see datasheet
 * For vectors, refer to <avr/iom328p.h>
 * For more on interrupts handling with AVR-GCC see
 * https://www.nongnu.org/avr-libc/user-manual/group__avr__interrupts.html
 */
ISR(TIMER2_OVF_vect, ISR_BLOCK)
{
	TCCR2B &= ~((1 << CS22) | (1 << CS21) | (1 << CS20)); /* stop timer */
	/* It's often required to manually reset interrupt flag */
        /* to avoid infinite processing of it.                  */
        /* not on AVRs (unless OCB bit set)                     */
        /* 	TIFR2 &= ~TOV2;                                 */
}

void sleep_ms(uint16_t ms_val)
{
	/* Set Power-Save sleep mode */
	/* https://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html */
	set_sleep_mode(SLEEP_MODE_IDLE);
	cli();		/* Disable interrupts -- as memory barrier */
	sleep_enable();	/* Set SE (sleep enable bit) */
	sei();  	/* Enable interrupts. We want to wake up, don't we? */
	TIMSK2 |= (1 << TOIE2); /* Enable Timer2 Overflow interrupt by mask */
	while (ms_val--) {
		/* Count 1 ms from TCNT2 to 0xFF (up direction) */
		TCNT2 = (uint8_t)(0xFF - (F_CPU / 128) / 1000);

		/* Enable Timer2 */
		TCCR2B =  (1 << CS22) | (1 << CS20); /* f = Fclk_io / 128, start timer */

		sleep_cpu();	/* Put MCU to sleep */

		/* This is executed after wakeup */

	}
	sleep_disable();	/* Disable sleeps for safety */		
}

static struct segm_Port PB = {
	.DDR = &DDRB,
	.PIN = &PINB,
	.PORT = &PORTB,
};

static struct segm_Display display = {
	.SHCP = {.port = &PB, .pin = 0},
	.STCP = {.port = &PB, .pin = 1},
	.DS   = {.port = &PB, .pin = 2},
	.delay_func = &_delay_loop_1,	/* 3 cycles / loop, busy wait */
	.sleep_ms_func = &sleep_ms,	/* 3 cycles / loop, busy wait */
	.is_comm_anode = false		/* We have common cathode display */
};

 /*Initializing an Interrupt (seconds account)*/
void t1_init()
{
	/* Adjust the divisor (CLK/256)*/
	TCCR1A = 0x00;
	TCCR1B = (1<<CS12)|(0<<CS11)|(0<<CS10);
	/* Counting to 62500 (1 sec))*/
	TCNT1 = 0;
	OCR1B = 62500;
	/*Enable timer overflow interrupt*/
	 TIMSK1 |= (1 << TOIE1);
	/* set the general interrupt enable bit*/
	sei();
}

	/* Interrupt for count seconds */
ISR(TIMER1_OVF_vect)
{
	point ^= 1;
	second++;
	if (second >= 60){   
		minute++;
		second = 0;
	}
	if (minute >= 60){
		hour++;
		minute = 0;
	}
	if (hour >= 24){
		hour=0;
	}
}

void int_ini(void)
{
	/* Enable interrupts INT0 on a falling edge */
	EICRA |= (1<<ISC01) | (0 << ISC00) | (1 << ISC11);
	/* allow external interrupts INT0 */
	EIMSK |= (1<<INT0) | (1 << INT1);
}

/* External interrupt for alarm*/
ISR(INT0_vect)
{	/* Setting an alarm hours */
uint8_t arr_al[] = {0, 0, 0, 0};
						
	alarm_hr++;
	if (alarm_hr >= 24){
		alarm_hr = 0;
	}
	BIN2BCD(arr_al, alarm_hr);
	for (int i = 0; i < 4; i++) {
		arr_al[i] = segm_sym_table[arr_al[i]];
	}
	for (int j = 0; j < 50; j++)
		segm_indicate4(&display, arr_al);
}
/* external interrupt for alarm */
ISR(INT1_vect)
{	/* setting an alarm minutes */
	uint8_t arr_al[] = {0, 0, 0, 0};
	alarm_min++;
	if (alarm_min >= 60){
		alarm_min = 0;
	}
	BIN2BCD(arr_al+2, alarm_min);
	for (int i = 0; i < 4; i++) {
		arr_al[i] = segm_sym_table[arr_al[i]];
	}
	for (int j = 0; j < 50; j++)
	segm_indicate4(&display, arr_al);
}

int main(void)
{	
	DDRB |= (1 << 3);
	PORTB |= (1 << 3);
	segm_init(&display);						/* Display initialization			*/
	minute = 0;							/** Setting start value for time and alarm	*/
	hour = 0;							/**						*/
	alarm_hr = 0;							/**						*/
	alarm_min = 0;							/**						*/
	uint8_t arr[] = {0, 0, 0, 0};					/* Array for time (hours and minutes)		*/
	t1_init();							/* Timer  for seconds initialization		*/
	int_ini();							/* Interrupts for setting alarm initialization	*/
	
	DDRD |= (0 << 2) | (0 << 3);					/** Setting alarm buttons			*/
	PORTD |= (1 << 2) | (1 << 3);					/** for interruption				*/
	while (1) {
		
		BIN2BCD(arr+2, minute);					/** BCD transform				 */
		BIN2BCD(arr, hour);					/**						 */
		for (int i = 0; i < 4; i++) {
			arr[i] = segm_sym_table[arr[i]];
		}
		if (point == 1)						/* Point between clocks and minutes		*/
			arr[1] |= (1 << 7);
		else
			arr[1] &= ~(1 << 7);
							
		segm_indicate4(&display, arr);				/* Clock indicate				 */
		
		DDRD |= (0 << 4) | (0 << 5);				/** Setting clock buttons			 */
		PORTD |= (1 << 4) | (1 << 5);				/**						 */
		if ((PIND & (1 << 4)) == (0 << 4)){			/* Setting hours manually			 */
			sleep_ms(100);
			if ((PIND & (1 << 4)) == (0 << 4))
				hour++;
			if (hour >= 24)
				hour = 0;
		}
		
		if ((PIND & (1 << 5)) == (0 << 5)){			/* Setting minutes manually			 */
			sleep_ms(100);
			if ((PIND & (1 << 5)) == (0 << 5))
				minute++;
			if (minute >= 60)
				minute = 0;
			
		}
	
		if ((hour == alarm_hr) && (minute == alarm_min))	/* Checking alarm time				*/
		{
			DDRD |= (1 << 7);
			PORTD|= (1 << 7);
		}
		else
			PORTD &= ~(1 << 7);
		
	}
}