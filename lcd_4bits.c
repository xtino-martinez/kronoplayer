/*
 * Delta mp3 player
 * Firmware V0.1 CC http://www.lestmar.com/
 * This software is licensed under Creative Commons 3.0 in the the form of by-nc, wich means 
 * that the use of this code is limited for non-commercial use. Any other use is permitted.
 * The routines used for MMC/SD and mp3 chip are licensed by Raphael Abrahams with different
 * restrictions (please check teuthis.com).
 */

// $Id: lcd_4bits.c 51 2010-12-21 17:39:24Z Tino $

#include "lcd.h"

#byte LCD_LAT	= 	0xF89 // Port A
#byte LCD_PORT	= 	0xF80
#byte LCD_TRIS	= 	0xF92

#byte CTRL_LAT	 	= 0xF8C // Port D
#byte CTRL_PORT		= 0xF83
#byte CTRL_TRIS		= 0xF95


#bit  lcd4_rs = CTRL_LAT.2
#bit  lcd4_rw = CTRL_LAT.3
#bit  lcd4_e 	= CTRL_LAT.4

#bit  lcd4_t_rs = CTRL_TRIS.2
#bit  lcd4_t_rw = CTRL_TRIS.3
#bit  lcd4_t_e 	= CTRL_TRIS.4


#bit busy_flag = LCD_PORT.7

static void lcd_post3();
static void lcd_busy();



static void lcd_post3()
{
    lcd4_rs = 0;
    lcd4_rw = 0;
    LCD_LAT = 0x3;
    lcd4_e = 1;
    lcd4_e = 0;
}



void lcd_init() {

    lcd4_t_rs = 0;
    lcd4_t_rw = 0;
    lcd4_t_e = 0;

    lcd4_rs = 0;
    lcd4_rw = 0;
    lcd4_e = 0;

	LCD_TRIS &= 0xF0;
	LCD_LAT &= 0xF0;


	delay_ms(15);
	lcd_post3();
    delay_ms(5);
	lcd_post3();
    delay_ms(1);
	lcd_post3();
	delay_ms(1);
	lcd_post3();
	delay_ms(1);


    lcd_putcmd(0x28); // 8-bit mode 0x28
	delay_ms(1);
    lcd_putcmd(0x8); // Display off 0x8
	delay_ms(1);
    lcd_putcmd(0xC); // Display on, curs.off, no-blink, 0xC
	delay_ms(1);
    lcd_putcmd(0x6); // Entry Mode 0x06
	delay_ms(1);
    lcd_putcmd(0x1); // ?
	delay_ms(10);
}

static void lcd_busy() {
    char s = 1;
    int i;



    LCD_TRIS |= 0x0F;
    lcd4_rs = 0;
    lcd4_rw = 1;


    for (i=0;i<5;i++) {
			
#asm
			NOP
#endasm


			/* delay_cycles(10);	// 50ns */
    		lcd4_e = 1;
#asm
			NOP
#endasm
			/*delay_cycles(1); */
        	s = busy_flag;
        	lcd4_e = 0;
#asm
			NOP
#endasm

        if (s)
        	break;		        
    }
    lcd4_rw = 0;
    LCD_TRIS &= 0xF0;

}

void lcd_putcmd(unsigned char c) {

    lcd_busy();
    lcd4_rs = 0;
    lcd4_rw = 0;
	LCD_LAT &= 0xF0;
	LCD_LAT |= c >> 4;
	lcd4_e = 1;
	lcd4_e = 0;

	LCD_LAT &= 0xF0;
	LCD_LAT |= c;
	lcd4_e = 1;
	lcd4_e = 0;

	delay_us(30);
}


void lcd_clear() {
    lcd_putcmd(0x01); 	// Clear Screen
	delay_ms(2);
}

void lcd_putchar(unsigned char c) {


    lcd_busy();
    lcd4_rs = 1;
    lcd4_rw = 0;
	LCD_LAT &= 0xF0;
	LCD_LAT |= c >> 4;
    lcd4_e = 1;	
    lcd4_e = 0;

	LCD_LAT &= 0xF0;
	LCD_LAT |= c;
	lcd4_e = 1;
	lcd4_e = 0;

	delay_us(30);
}


