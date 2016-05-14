/*
 * Delta mp3 player
 * Firmware V0.1 CC http://www.lestmar.com/
 * This software is licensed under Creative Commons 3.0 in the the form of by-nc, wich means 
 * that the use of this code is limited for non-commercial use. Any other use is permitted.
 * The routines used for MMC/SD and mp3 chip are licensed by Raphael Abrahams with different
 * restrictions (please check teuthis.com).
 */

// $Id: lcd.h 21 2010-07-28 08:38:59Z Tino $


#ifndef LCD_H
#define LCD_H

void lcd_init();
void lcd_putcmd(unsigned char c);
void lcd_clear();
void lcd_putchar(unsigned char c);

#endif