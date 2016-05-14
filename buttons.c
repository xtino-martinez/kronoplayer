/*
 * Delta mp3 player
 * Firmware V0.1 CC http://www.lestmar.com/
 * This software is licensed under Creative Commons 3.0 in the the form of by-nc, wich means 
 * that the use of this code is limited for non-commercial use. Any other use is permitted.
 * The routines used for MMC/SD and mp3 chip are licensed by Raphael Abrahams with different
 * restrictions (please check teuthis.com).
 */

// $Id: buttons.c 21 2010-07-28 08:38:59Z Tino $

#define BUT_STATE_RELEASED 		0
#define BUT_STATE_DEBOUNCE_UP	1
#define BUT_STATE_PRESSED_ONCE 	2
#define BUT_STATE_REPEATING 	3
#define BUT_STATE_DEBOUNCE_DOWN	4

//int button_state=BUT_STATE_RELEASED;

#define BUT_DEBOUNCE_TICKS		10
#define BUT_PRESS_ONCE_TICKS	180
#define BUT_PRESS_REPEAT		25


typedef unsigned int1(*t_button_rdport)();
typedef void (*t_button_action)(int1 repeat); 

struct button_struct {
	t_button_rdport rdport;
	int state;
	int clock;
	t_button_action action;
};

int1 button_poll(struct button_struct* b)
{
	int1 v=b->rdport();
	int1 isPressed=0;

	switch( b->state )
	{
		case BUT_STATE_RELEASED:
		
	    if (!v) {
			b->clock=BUT_DEBOUNCE_TICKS;
			b->state=BUT_STATE_DEBOUNCE_UP;
			b->action(0);
			isPressed=1;

		}
		break;
		case BUT_STATE_DEBOUNCE_UP:
			if( b->clock>0 )
			{
				--b->clock;
			}
			else {					
		    	if (v)
				{
					b->state=BUT_STATE_DEBOUNCE_DOWN;
					b->clock=BUT_DEBOUNCE_TICKS;
				}
				else
				{
					b->state=BUT_STATE_PRESSED_ONCE;
					b->clock=BUT_PRESS_ONCE_TICKS;
				}
			}
		break;
		case BUT_STATE_PRESSED_ONCE:

			if( v )
			{
				b->state = BUT_STATE_DEBOUNCE_DOWN;
				b->clock=BUT_DEBOUNCE_TICKS;
			}
			else
			{
				if( b->clock>0 )
					--b->clock;
				else {
					b->state = BUT_STATE_REPEATING;
					b->clock=BUT_PRESS_REPEAT;
					b->action(1);
					isPressed=1;
				}
			}
		break;
		case BUT_STATE_REPEATING:
			if( v )
			{
				b->state = BUT_STATE_DEBOUNCE_DOWN;
				b->clock=BUT_DEBOUNCE_TICKS;
			}
			else
			{
				if( b->clock>0 )
					--b->clock;				
				else 
				{
					b->clock=BUT_PRESS_REPEAT;
					b->action(1);
					isPressed=1;
				}
			}
		break;
		case BUT_STATE_DEBOUNCE_DOWN:
			if( b->clock>0 )
				--b->clock;		
			else
				b->state = BUT_STATE_RELEASED;
		break;
	}	

	return (b->state != BUT_STATE_RELEASED);
}
