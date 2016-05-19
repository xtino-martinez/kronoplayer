/*
 * Delta mp3 player
 * Firmware V0.1 CC http://www.lestmar.com/
 * This software is licensed under Creative Commons 3.0 in the the form of by-nc, wich means 
 * that the use of this code is limited for non-commercial use. Any other use is permitted.
 * The routines used for MMC/SD and mp3 chip are licensed by Raphael Abrahams with different
 * restrictions (please check teuthis.com).
 */

// $Id: main.c 63 2011-05-15 18:14:12Z ecelema $


#include <18f45j10.H>
#include <stdlib.h>
#fuses H4_SW,NOWDT,DEBUG
#use delay(clock=40,000,000)
//was 24mhz

//#use rs232(baud=9600, xmit=PIN_C6,rcv=PIN_C7)

#define DELTA_DEBUG


//////////.////////////////.////////////////.///////////////./////////////////

int error=0;  	   	  
  	   	   	  
#byte TRIS_E	 = 0xF96
#byte TRIS_D	 = 0xF95
#byte TRIS_A	 = 0xF92

#byte LAT_E	 = 0xF8D
#byte LAT_D	 = 0xF8C
#byte LAT_A	 = 0xF89

#byte PORT_E	 = 0xF84
#byte PORT_D	 = 0xF83
#byte PORT_A	 = 0xF80

//some useful bits:

#byte OSCTUNE = 0xF9B
#bit PLLEN = OSCTUNE.6

#define FLASH_ALARM_ADDR 0x7800

#rom int FLASH_ALARM_ADDR ={ 	
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 1 }


////////////////////////////////////////////////////////////////////////////////////////////////

// Common variables
int32 currentsong=1;

int32 current_ccl;
int32 current_songlength;

unsigned int16 fp_file_entry;
int1 end_of_directory=0;

#define PLAYER_MODE_STOPPED			0
#define PLAYER_MODE_MUSIC_NO_BELL  	1
#define PLAYER_MODE_MUSIC_BELL  	2
#define PLAYER_MODE_BELL_INI		3
#define PLAYER_MODE_BELL_FIN		4
#define PLAYER_MODE_PAUSED			5

int1 random_music=0;
unsigned char current_alarm=0;

int player_mode = PLAYER_MODE_STOPPED;
int1 player_one_song = 0;

unsigned char window_pos=0;


#define	FM_STATE_CLOCK      0
#define	FM_STATE_HSET       1
#define	FM_STATE_MSET       2
#define	FM_STATE_ALARM      3
#define FM_STATE_ALON       4
#define FM_STATE_ALH        5
#define FM_STATE_ALM        6
#define FM_STATE_ALDM       7
#define FM_STATE_ALDS       8
#define FM_STATE_ALT        9
#define	FM_STATE_PLAYER     10
#define	FM_STATE_PLAYING    11
#define	FM_STATE_PAUSED    	12
#define	FM_STATE_ALARM_ON  	13
#define	FM_STATE_SKIP    	14

int state = 0;

int1 enable_sd_check=1;
void check_clock(int1 f);
void lcd_printhour();
void lcd_printready();
void player_play_bell_ini();
void player_play_bell_fin();

#include "lcd_4bits.c"
#include "rtc.c"
#include "buttons.c"
#include "teuthis.c"

//#define LCD_ENABLE_SEC 0

#byte BUT_PORT	= 0xF83 // Port D
#byte BUT_TRIS	= 0xF95

#byte BUT_PORT2	= 0xF84 // Port E
#byte BUT_TRIS2	= 0xF96

/*
#byte LED_LAT = 0xF8A // Port B
#byte LED_TRIS = 0xF93

#bit  led_test = LED_LAT.5
#bit  led_t_test = LED_TRIS.5
*/

#bit 	but_mode 	= BUT_PORT.5
#bit 	but_set 	= BUT_PORT.6
#bit 	but_up 		= BUT_PORT2.0
#bit 	but_down 	= BUT_PORT.7
#bit 	but_stop	= BUT_PORT2.1


#bit 	but_t_mode 	= BUT_TRIS.5
#bit 	but_t_set 	= BUT_TRIS.6
#bit 	but_t_up 	= BUT_TRIS2.0
#bit 	but_t_down 	= BUT_TRIS.7
#bit 	but_t_stop	= BUT_TRIS2.1




unsigned char hh = 0;
unsigned char mm = 0;
unsigned char sec=0;

unsigned char dd = 0;
unsigned char _MM = 0;
unsigned char yy = 0;


struct alarm {
    unsigned int on;
    unsigned int hh;
    unsigned int mm;
    unsigned int d_mm;
    unsigned int d_ss;
    unsigned int track;
};

struct alarm_x {
    unsigned int state;
	int32 start;
	int32 end;
	int1 overnight;
	int1 canceled;
};

#define ALARM_STATE_OFF 0
#define ALARM_STATE_ON 	1

#define N_ALARMS 9

struct alarm al[N_ALARMS];
struct alarm_x alx[N_ALARMS];
int cur_alarm = 0;
int title_delay=3;

// just for debug: measure time spent in a code chunk
//long meas_time=0;
int1 isUpOrDown();

int16 stat_nsongs;
int16 stat_highest; 
int16 stat_highest_i; 

int1 deferred_up = 0;
int1 deferred_down = 0;

#define AL_CFG_OPT 5
#define AL_CFG_LEN 8

#define VOLUME_BELL 255
#define VOLUME_SONG 245

const char alarm_options[AL_CFG_OPT][AL_CFG_LEN] = 
	{ "OFF      ",
	  "SEQ      ",
	  "RND      ",
	  "Bell+SEQ ",
	  "Bell+RND " };			


#define ALARM_OPTION_OFF			0
#define ALARM_OPTION_TRACK			1
#define ALARM_OPTION_RND			2
#define ALARM_OPTION_BELL_TRACK		3
#define ALARM_OPTION_BELL_RND		4

int32 power_on_hour=86399LL;
int32 power_off_hour=0;
int32 power_off_lineal=0;
int1 power_on_enabled=0;
int1 power_overnight=0;

void update_alarm_expiry(struct alarm* a, struct alarm_x* ax)
{
	int32 lineal_end=0;
	
	ax->start = 3600LL*a->hh+60LL*a->mm;
	ax->end = 3600LL*a->d_mm+60LL*a->d_ss;
	
	ax->overnight = ax->start > ax->end;

	lineal_end = ax->end;

	if( ax->overnight)
	{
		lineal_end += 86400LL;
	}

	if( a->on )
	{


		if ( ((ax->start+86340L) % 86400LL) < power_on_hour )
			power_on_hour = ((ax->start+86340L) % 86400LL);

		if( (lineal_end+60) > power_off_lineal )
		{
			power_off_lineal = lineal_end+60;
			if ( power_off_lineal >= 86400LL )
			{
				power_off_hour = power_off_lineal - 86400LL;
			} 
			else
			{
				power_off_hour = power_off_lineal;
			}

			if( power_off_hour < power_on_hour )
			{
				power_overnight = 1;
			}
		}
	
		power_on_enabled=1;
	}

}


void save_alarms()
{
	int i;

	power_on_enabled=0;
	power_overnight=0;
	power_on_hour=86399LL;
	power_off_hour=0;
	power_off_lineal=0;

	write_program_memory(FLASH_ALARM_ADDR,al,sizeof(al));
	
	for( i=0; i<N_ALARMS; i++ )
	{
		update_alarm_expiry(&al[i],&alx[i]);
	}
}

void load_alarms()
{
	int i;

	power_on_enabled=0;
	power_overnight=0;
	power_on_hour=86399LL;
	power_off_hour=0;
	power_off_lineal=0;

	read_program_memory(FLASH_ALARM_ADDR,al,sizeof(al));
	
	for( i=0; i<N_ALARMS; i++ )
	{
		update_alarm_expiry(&al[i],&alx[i]);
	}
}


				

void lcd_printtrackplnumber(struct t_dir_entry* entry,unsigned char add,int1 rot, int scrlen);

int1 seek_track( 	int16* track,
					int16 highest_i,
					int16* fp_file_entry,
					int32* ccl,
					int32* songlength );

int1 seek_next( int16* track,
				int32* ccl,
				int32* songlength,
				int1 end_of_directory );

int1 seek_rand( int32* ccl,
				int32* songlength );

void lcd_playing_title();
void lcd_printtrackpllist();

void player_stop()
{
	player_mode = PLAYER_MODE_STOPPED;	
}

void player_play_sequential(int16 track, int1 bell)
{
	if( bell)
		player_mode = PLAYER_MODE_MUSIC_BELL;
	else
		player_mode = PLAYER_MODE_MUSIC_NO_BELL;

	end_of_directory = seek_track( 	&track,
									stat_highest_i,
									&fp_file_entry,
									&current_ccl,
									&current_songlength );

	currentsong = track;
}

void player_play_random(int1 bell)
{
	if( bell)
		player_mode = PLAYER_MODE_MUSIC_BELL;
	else
		player_mode = PLAYER_MODE_MUSIC_NO_BELL;

	end_of_directory = seek_rand( 	&current_ccl,
									&current_songlength );

}

void player_play_bell_ini()
{
	player_mode = PLAYER_MODE_BELL_INI;
	//volume=VOLUME_BELL;
}

void player_play_bell_fin()
{
	player_mode = PLAYER_MODE_BELL_FIN;
	//volume=VOLUME_BELL;
}

void player_end_of_song()
{
	switch( player_mode )
	{
		case PLAYER_MODE_MUSIC_NO_BELL:
		case PLAYER_MODE_MUSIC_BELL:
			//volume=VOLUME_SONG;

			if( player_one_song )
			{
				player_mode = PLAYER_MODE_STOPPED;
				player_one_song = 0;
			}
			else 
			{
				if (random_music)
				{
					if( player_one_song )
					{
						player_mode = PLAYER_MODE_STOPPED;
						player_one_song = 0;
					}
					else 
					{
						end_of_directory = seek_rand( 	&current_ccl,
													&current_songlength );
					}
					lcd_playing_title();
				}
				else
				{
					end_of_directory = seek_next( 	&currentsong,
													&current_ccl,
													&current_songlength,
													end_of_directory );				
				
					if( state == FM_STATE_ALARM_ON)
						lcd_playing_title();
					else
					{
						if( fp_file_entry == 0 )	
							window_pos = fp_file_entry;
						else
							window_pos = fp_file_entry-1;
						lcd_printtrackpllist();
					}
				}
			}
			
			break;
		case PLAYER_MODE_BELL_INI:
			if( random_music )
			{
				//volume=VOLUME_SONG;
				player_play_random(true);
				lcd_playing_title();
			}
			else
			{
				player_play_sequential(currentsong, true);
				lcd_playing_title();
			}
			break;
		case PLAYER_MODE_BELL_FIN:
			state = FM_STATE_CLOCK;
			lcd_clear();
			lcd_printhour();
			lcd_printready();
			alx[current_alarm].state = ALARM_STATE_OFF;
			player_mode = PLAYER_MODE_STOPPED;
			break;
	}	
}

void lcd_blinkhour(int f) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x85); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + hh / 10);
        lcd_putchar('0' + hh % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}

void lcd_blinkcolon(int f) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x87); // Set the cursor in 1st row, 5th column

    if (on)
        lcd_putchar(':');
    else
        lcd_putchar(' ');

    on = !on;

}

void lcd_blinkminute(int f) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x88); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + mm / 10);
        lcd_putchar('0' + mm % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}

void lcd_printhour() {
    lcd_putcmd(0x85); // Set the cursor in 1st row, 5th column
    lcd_putchar('0' + hh / 10);
    lcd_putchar('0' + hh % 10);
    lcd_putchar(':');
    lcd_putchar('0' + mm / 10);
    lcd_putchar('0' + mm % 10);
#ifdef LCD_ENABLE_SEC
    lcd_putchar('.');
    lcd_putchar('0' + sec / 10);
    lcd_putchar('0' + sec % 10);
#endif
}

#if 0
void lcd_printdate() {
    lcd_putcmd(0xC3); // Set the cursor in 2nd row, 3rd column
    lcd_putchar('0' + dd / 10);
    lcd_putchar('0' + dd % 10);
    lcd_putchar('/');
    lcd_putchar('0' + _MM / 10);
    lcd_putchar('0' + _MM % 10);
    lcd_putchar('/');
    lcd_putchar('2');
    lcd_putchar('0');
    lcd_putchar('0' + yy / 10);
    lcd_putchar('0' + yy % 10);
}
#endif


void lcd_boot_screen() {

    lcd_putcmd(0x82); // Set the cursor in 2nd row, 3rd column

    lcd_putchar('K');
	lcd_putchar('r');
	lcd_putchar('o');
	lcd_putchar('n');
	lcd_putchar('o');
	lcd_putchar('P');
	lcd_putchar('l');
	lcd_putchar('a');
	lcd_putchar('y');
	lcd_putchar('e');
	lcd_putchar('r');

    lcd_putcmd(0xC1); // Set the cursor in 2nd row, 3rd column
    lcd_putchar('f');
    lcd_putchar('i');
    lcd_putchar('r');
    lcd_putchar('m');
    lcd_putchar('w');
    lcd_putchar('a');
    lcd_putchar('r');
    lcd_putchar('e');
    lcd_putchar(' ');
	lcd_putchar('1');
    lcd_putchar('.');
	lcd_putchar('0');
	lcd_putchar('9');
}


void lcd_playing_alarm(int i) {
    lcd_putcmd(0x82); // Set the cursor in 2nd row, 3rd column
    lcd_putchar('P');
    lcd_putchar('l');
    lcd_putchar('a');
    lcd_putchar('y');
    lcd_putchar('i');
    lcd_putchar('n');
    lcd_putchar('g');
    lcd_putchar(' ');
    lcd_putchar('P');
    lcd_putchar('R');
	lcd_putchar('0'+i+1);
}

void lcd_playing_title() {


	if( player_mode == PLAYER_MODE_BELL_INI ||
		player_mode == PLAYER_MODE_BELL_FIN )
	{
	    lcd_putcmd(0xC6); // Set the cursor in 2nd row, 3rd column
	    lcd_putchar('B');
	    lcd_putchar('e');
	    lcd_putchar('l');
	    lcd_putchar('l');
	} else {
		lcd_putcmd(0xC6);
		lcd_printtrackplnumber(&fp_dir_entries[fp_file_entry],0xC0,0,16);		
	}
}


void lcd_printalarm(int i) {

	int j;

    lcd_putcmd(0x80); 
    lcd_putchar('P');
    lcd_putchar('R');
    lcd_putchar('0' + i + 1);
    lcd_putchar(' ');

	if(al[i].on)
		{
	    lcd_putchar('0' + al[i].hh / 10);
	    lcd_putchar('0' + al[i].hh % 10);
	    lcd_putchar(':');
	    lcd_putchar('0' + al[i].mm / 10);
	    lcd_putchar('0' + al[i].mm % 10);
	    lcd_putchar(' ');
	    lcd_putchar('0' + al[i].d_mm / 10);
	    lcd_putchar('0' + al[i].d_mm % 10);
        lcd_putchar(':');
	    lcd_putchar('0' + al[i].d_ss / 10);
	    lcd_putchar('0' + al[i].d_ss % 10);
	  } else {
		for(j=0;j<AL_CFG_LEN;j++)
			lcd_putchar(alarm_options[al[i].on][j]);
		lcd_putchar(' ');
		lcd_putchar(' ');
		lcd_putchar(' ');		
	  }
}

void lcd_blinkaltrack(int f);
void lcd_printtrack(int i) {

	int16 track = al[i].track;

	end_of_directory = seek_track( 	&track,
									stat_highest_i,
									&fp_file_entry,
									&current_ccl,
									&current_songlength );

	lcd_printtrackplnumber(&fp_dir_entries[fp_file_entry],0xC1,0,14);
    lcd_blinkaltrack(1);
}


void lcd_blinkalhour(int f, int i) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x84); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + al[i].hh / 10);
        lcd_putchar('0' + al[i].hh % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}

void lcd_blinkalminute(int f, int i) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x87); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + al[i].mm / 10);
        lcd_putchar('0' + al[i].mm % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}

void lcd_blinkaldminute(int f, int i) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x8A); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + al[i].d_mm / 10);
        lcd_putchar('0' + al[i].d_mm % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}

void lcd_blinkaldsecond(int f, int i) {
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0x8D); // Set the cursor in 1st row, 5th column

    if (on) {
        lcd_putchar('0' + al[i].d_ss / 10);
        lcd_putchar('0' + al[i].d_ss % 10);
    }
    else {
        lcd_putchar(' ');
        lcd_putchar(' ');
    }

    on = !on;
}


void lcd_blinkaltrack(int f)
{
    static int on = 0;

    if (f)
        on = 1;

    lcd_putcmd(0xC9); // Set the cursor in 1st row, 5th column

    if (on) {
		lcd_putcmd(0xC0);
		lcd_putchar('>');
		lcd_putcmd(0xCF);
		lcd_putchar('<');
    } else {
		lcd_putcmd(0xC0);
		lcd_putchar(' ');
		lcd_putcmd(0xCF);
		lcd_putchar(' ');
    }

    on = !on;
}

void lcd_blinkalon(int f, int i)
{
	static int on = 0;
	int j;

    if (f)
        on = 1;

    lcd_putcmd(0x84); // Set the cursor in 1st row, 5th column

	if( on )
	{
		for( j=0;j<AL_CFG_LEN; j++)
			lcd_putchar(alarm_options[al[i].on][j]);
			lcd_putchar(' ');
			lcd_putchar(' ');
			lcd_putchar(' ');
	} else {
		for( j=0;j<AL_CFG_LEN+3; j++)
			lcd_putchar(' ');
	}

    on = !on;
}


void lcd_printready() {

#ifdef VOLUME_CTRL

	char vs[4];
	sprintf(vs,"%03u",volume);

#endif

	lcd_putcmd(0xC1); // Set the cursor in 2nd row, 3rd column
	lcd_putchar(' ');
	lcd_putchar(' ');
	lcd_putchar(' ');
	lcd_putchar('[');

#ifdef DELTA_DEBUG

	if (error==0)
	{
#endif

	lcd_putchar('R');
	lcd_putchar('e');
	lcd_putchar('a');
	lcd_putchar('d');
	lcd_putchar('y');

#ifdef DELTA_DEBUG
	} else {
		lcd_putchar('0'+error);
	}
#endif

	lcd_putchar(']');
	lcd_putchar(' ');
#ifdef VOLUME_CTRL
	lcd_putchar(vs[0]);
	lcd_putchar(vs[1]);
	lcd_putchar(vs[2]);
#else
	lcd_putchar(' ');
	lcd_putchar(' ');
	lcd_putchar(' ');
#endif
}

void lcd_printready_i(int16 n) {

	char x[6];
	sprintf(x,"%05lu",n);
	

	lcd_putcmd(0xC1); // Set the cursor in 2nd row, 3rd column
	lcd_putchar(' ');
	lcd_putchar(' ');
	lcd_putchar('*');
	lcd_putchar('*');
	lcd_putchar(x[0]);
	lcd_putchar(x[1]);
	lcd_putchar(x[2]);
	lcd_putchar(x[3]);
	lcd_putchar(x[4]);
	lcd_putchar('*');
	lcd_putchar('*');
	lcd_putchar(' ');
	lcd_putchar(' ');
	lcd_putchar(' ');

}



void print_error_no_titles()
{
    lcd_putcmd(0xC0); // Set the cursor in 2nd row, 3rd column
    lcd_putchar('N');
    lcd_putchar('o');
    lcd_putchar(' ');
    lcd_putchar('t');
    lcd_putchar('i');
    lcd_putchar('t');
    lcd_putchar('l');
    lcd_putchar('e');
    lcd_putchar('s');
    lcd_putchar(' ');
    lcd_putchar('f');
	lcd_putchar('o');
	lcd_putchar('u');
	lcd_putchar('n');	
	lcd_putchar('d');	
	lcd_putchar('!');	
}

void print_error_no_sd_card()
{
    lcd_putcmd(0xC2); // Set the cursor in 2nd row, 3rd column
    lcd_putchar('N');
    lcd_putchar('o');
    lcd_putchar(' ');
    lcd_putchar('S');
    lcd_putchar('D');
    lcd_putchar(' ');
    lcd_putchar('C');
    lcd_putchar('a');
    lcd_putchar('r');
    lcd_putchar('d');
	lcd_putchar('!');	
}


void lcd_printtrackplnumber(struct t_dir_entry* entry,unsigned char add,int1 rot, int scrlen)
{

	int c=scrlen;
	int len;

	if( !rot )
		entry->tit_offset = 0;

	if( entry->tit_full )
		len = 32;
	else
		len = 31 - entry->tit_p + 1;

	int i;
	lcd_putcmd(add);


	if( entry->tit_full )
	{
		for(i=entry->tit_p+entry->tit_offset+1;i<entry->tit_p+entry->tit_offset+scrlen+1 && c;i++,c--)
		{
			lcd_putchar(entry->title[i%32]);
		}	
	}
	else
	{
		for(i=entry->tit_p+entry->tit_offset+1;i<32 && c;i++,c--)
		{
			lcd_putchar(entry->title[i]);
		}
	
	
		for(i=entry->tit_p;i<entry->tit_offset+entry->tit_p && c;i++,c--)
		{
			lcd_putchar(entry->title[i]);
		}			
	}


	for(i=len;i<scrlen+1;i++)
		lcd_putchar(' ');

	
	if( rot && len > scrlen )
		fp_dir_entries[fp_file_entry].tit_offset = (fp_dir_entries[fp_file_entry].tit_offset+1) % len;

}

void lcd_printtrackpllist()
{	
	lcd_putcmd(0x80);
	if( fp_file_entry==window_pos )
		lcd_putchar('>');
	else
		lcd_putchar(' ');

	lcd_printtrackplnumber(&fp_dir_entries[window_pos],0x81,0,14); 

	lcd_putcmd(0x8F);
	if( fp_file_entry==window_pos )
		lcd_putchar('<');
	else
		lcd_putchar(' ');		

	if( fp_dir_nentries > 1)
	{
		
		lcd_putcmd(0xC0);
		if( fp_file_entry!=window_pos )
			lcd_putchar('>');
		else
			lcd_putchar(' ');
	
		lcd_printtrackplnumber(&fp_dir_entries[window_pos+1],0xC1,0,14);
	
		lcd_putcmd(0xCF);
		if( fp_file_entry!=window_pos )
			lcd_putchar('<');
		else
			lcd_putchar(' '); 
	} 
}



#define high_start 76

void action_mode(int1 repeat) {

	if(!repeat)
	{
	    switch (state) {
	        case FM_STATE_CLOCK:
	            state = FM_STATE_ALARM;
	    		lcd_clear();
	            lcd_printalarm(cur_alarm);
	            lcd_printtrack(cur_alarm);
	            break;
	        case FM_STATE_HSET:
				set_hour(hh,mm,0 );
	            state = FM_STATE_ALARM;
				lcd_clear();
	            lcd_printalarm(cur_alarm);
	            lcd_printtrack(cur_alarm);
	            break;
	        case FM_STATE_MSET:
	            set_hour(hh,mm,0 );
	            state = FM_STATE_ALARM;
				lcd_clear();
	            lcd_printalarm(cur_alarm);
	            lcd_printtrack(cur_alarm);
	            break;
	        case FM_STATE_ALARM:
	            state = FM_STATE_PLAYER;
				lcd_clear();
				lcd_printtrackpllist();
	            break;
	        case FM_STATE_ALON:
	        case FM_STATE_ALH:
	        case FM_STATE_ALM:
	        case FM_STATE_ALDM:
	        case FM_STATE_ALDS:
	        case FM_STATE_ALT:
	            save_alarms();
	            state = FM_STATE_PLAYER;
				lcd_clear();
				lcd_printtrackpllist();
	            break;
	        case FM_STATE_PLAYER:
			case FM_STATE_PLAYING:
			case FM_STATE_PAUSED:
				player_stop();
				play_led = 1;
	            state = FM_STATE_CLOCK;	            
				lcd_clear();
	            lcd_printhour();
	            lcd_printready();
	            break;
	    }
	}
}

void action_set(int1 repeat) {

	if(!repeat)
	{
	    switch (state) {
	        case FM_STATE_CLOCK:
	            lcd_blinkcolon(1);
	            state = FM_STATE_HSET;
				//set_hour(hh,mm,0 );
	            break;
	        case FM_STATE_HSET:
	            lcd_blinkhour(1);
	            state = FM_STATE_MSET;
				set_hour(hh,mm,0 );
	            break;
	        case FM_STATE_MSET:
	            lcd_blinkminute(1);
	            state = FM_STATE_CLOCK;
				set_hour(hh,mm,0 );
	            break;
	        case FM_STATE_ALARM:
	            state = FM_STATE_ALON;
				lcd_printalarm(cur_alarm);
	            break;
	        case FM_STATE_ALON:
	        		if (al[cur_alarm].on)
	            	state = FM_STATE_ALH;
	            else
	            	state = FM_STATE_ALARM;
	            save_alarms();
	            lcd_printalarm(cur_alarm);
	            break;
	        case FM_STATE_ALH:
	            state = FM_STATE_ALM;
	            //save_alarm(	cur_alarm,&al[cur_alarm]);
	            lcd_blinkalhour(1,cur_alarm);
	            break;
	        case FM_STATE_ALM:
	            state = FM_STATE_ALDM;
	            //save_alarm(	cur_alarm,&al[cur_alarm]);
	            lcd_blinkalminute(1,cur_alarm);
	            break;
	        case FM_STATE_ALDM:
	            state = FM_STATE_ALDS;
	            //save_alarm(	cur_alarm,&al[cur_alarm]);
	            lcd_blinkaldminute(1,cur_alarm);
	            break;
	        case FM_STATE_ALDS:
	            state = FM_STATE_ALT;
	            //save_alarm(	cur_alarm,&al[cur_alarm]);
	            lcd_blinkaldsecond(1,cur_alarm);
	            break;
	        case FM_STATE_ALT:
	            save_alarms();
	            state = FM_STATE_ALARM;
	            lcd_blinkaltrack(1);
	            break;
	    }
	}
}

int1 alarm_being_configured( int i)
{
	return ( 	((state == FM_STATE_ALT ||
				state == FM_STATE_ALDS ||
				state == FM_STATE_ALDM ||
				state == FM_STATE_ALH ||
				state == FM_STATE_ALM ||
				state == FM_STATE_ALON) &&
				i == cur_alarm) || 
				(state == FM_STATE_HSET ||
				 state == FM_STATE_MSET ) );
}


int1 move_title_up(int1 player_mode)
{
	if( curr_window>1 || fp_file_entry>0 )
	{
		if( window_pos==0 && fp_file_entry==0)
		{
			prev_dir_buffer();
			window_pos=MAX_DIR_ENTRY-1;
			fp_file_entry=MAX_DIR_ENTRY-1;
			end_of_directory=0;
		} else {
			if( fp_file_entry==window_pos )
				--window_pos;	
			--fp_file_entry;					
		}	
		title_delay=3;
		if( player_mode)
			lcd_printtrackpllist();
		else
			lcd_printtrackplnumber(&fp_dir_entries[fp_file_entry],0xC1,0,14);
		return 1;
	} 
	return 0;
}


void action_up(int1 repeat) {

    switch (state) {

#ifdef VOLUME_CTRL

	    case FM_STATE_CLOCK:
        case FM_STATE_PLAYING:
			if(volume < 255 ) ++volume;
			lcd_printready();
			break;
#else
        case FM_STATE_PLAYING:

			if( curr_window>1 || fp_file_entry>0 )
			{
				deferred_up = 1;
				state = FM_STATE_SKIP;
			}
			break;

#endif

        case FM_STATE_HSET:
            hh = (hh + 1) % 24;
            lcd_blinkhour(1);
            break;
        case FM_STATE_MSET:
            mm = (mm + 1) % 60;
            lcd_blinkminute(1);
            break;
        case FM_STATE_ALARM:
            cur_alarm = (cur_alarm+1) % N_ALARMS;
            lcd_printalarm(cur_alarm);
            lcd_printtrack(cur_alarm);
            break;
        case FM_STATE_ALH:
        		al[cur_alarm].hh = (al[cur_alarm].hh + 1) % 24;
        		lcd_blinkalhour(1,cur_alarm);
            break;
        case FM_STATE_ALM:
        	  al[cur_alarm].mm = (al[cur_alarm].mm + 1) % 60;
        	  lcd_blinkalminute(1,cur_alarm);
            break;
        case FM_STATE_ALT:
				move_title_up(0);
        		al[cur_alarm].track = fp_dir_entries[fp_file_entry].track;
        		lcd_blinkaltrack(1);
            break;
        case FM_STATE_ALDM:
        	  al[cur_alarm].d_mm = (al[cur_alarm].d_mm + 1) % 24;
        	  lcd_blinkaldminute(1,cur_alarm);
            break;
        case FM_STATE_ALDS:
        	  al[cur_alarm].d_ss = (al[cur_alarm].d_ss + 1) % 60;
        	  lcd_blinkaldsecond(1,cur_alarm);
            break;
        case FM_STATE_ALON:
        	  al[cur_alarm].on = (al[cur_alarm].on + 1) % AL_CFG_OPT;
        	  lcd_blinkalon(1,cur_alarm);
            break;
        case FM_STATE_PLAYER:
				move_title_up(1);
            break;
    }
}

int1 move_title_down(int1 player_mode)
{	
	if( fp_dir_global_entries<stat_nsongs || fp_file_entry < (fp_dir_nentries-1) )
		 
	{			
		if( fp_file_entry==MAX_DIR_ENTRY )
		{
			end_of_directory=!next_dir_buffer();
			window_pos=0;
			fp_file_entry=1;
		} else {
			if( fp_file_entry!=window_pos )
				++window_pos;	
			++fp_file_entry;
		}		
		title_delay=3;
		
		if( player_mode)
			lcd_printtrackpllist();
		else
			lcd_printtrackplnumber(&fp_dir_entries[fp_file_entry],0xC1,0,14);

		return 1;
	}
	return 0;
}

void action_down(int1 repeat) {
    switch (state) {

#ifdef VOLUME_CTRL

	    case FM_STATE_CLOCK:
        case FM_STATE_PLAYING:
			if(volume > 0 ) --volume;
			lcd_printready();
			break;
#else
        case FM_STATE_PLAYING:

			if( fp_dir_global_entries<stat_nsongs || fp_file_entry < (fp_dir_nentries-1) )
			{
				deferred_down = 1;
				state = FM_STATE_SKIP;
			}

            break;
#endif

        case FM_STATE_HSET:
            hh = (hh == 0) ? 23 : (hh - 1);
            lcd_blinkhour(1);
            break;
        case FM_STATE_MSET:
            mm = (mm == 0) ? 59 : (mm - 1);
           	lcd_blinkminute(1);
            break;
        case FM_STATE_ALARM:
            cur_alarm = (cur_alarm==0) ? 8 : (cur_alarm-1);
            lcd_printalarm(cur_alarm);
            lcd_printtrack(cur_alarm);
            break;
        case FM_STATE_ALH:
        		al[cur_alarm].hh = (al[cur_alarm].hh==0) ? 23 : (al[cur_alarm].hh -1);
        		lcd_blinkalhour(1,cur_alarm);
            break;
        case FM_STATE_ALM:
        		al[cur_alarm].mm = (al[cur_alarm].mm==0) ? 59 : (al[cur_alarm].mm -1);
        		lcd_blinkalminute(1,cur_alarm);
            break;
        case FM_STATE_ALDM:
        		al[cur_alarm].d_mm = (al[cur_alarm].d_mm==0) ? 23 : (al[cur_alarm].d_mm -1);
        		lcd_blinkaldminute(1,cur_alarm);
            break;
        case FM_STATE_ALDS:
        		al[cur_alarm].d_ss = (al[cur_alarm].d_ss==0) ? 59 : (al[cur_alarm].d_ss -1);
        		lcd_blinkaldsecond(1,cur_alarm);
            break;
        case FM_STATE_ALT:
				move_title_down(0);
        		al[cur_alarm].track = fp_dir_entries[fp_file_entry].track;
        		lcd_blinkaltrack(1);
            break;
        case FM_STATE_ALON:

        		al[cur_alarm].on = (al[cur_alarm].on==0) ? (AL_CFG_OPT-1) : (al[cur_alarm].on-1);
        		lcd_blinkalon(1,cur_alarm);
            break;
        case FM_STATE_PLAYER:
				move_title_down(1);
            break;
    }
}

int1 seek_next( int16* track,
				int32* ccl,
				int32* songlength,
				int1 end_of_directory )
{
	if( fp_file_entry>=(fp_dir_nentries-1) )
	{
		if( end_of_directory || fp_dir_global_entries>=stat_nsongs)
			init_fat_buffer();
		end_of_directory=!next_dir_buffer();
		window_pos=0;
		if(curr_window==1)
		{
			fp_file_entry=0;
		} else {
			fp_file_entry=1;
		}
		
	} else {
		++fp_file_entry;
	}

	*ccl = fp_dir_entries[fp_file_entry].ccl;
	*songlength = fp_dir_entries[fp_file_entry].songlength;		
	*track = fp_dir_entries[fp_file_entry].track;

	return end_of_directory;
}

int1 seek_track( 	int16* track,
					int16 highest_i,
					int16* fp_file_entry,
					int32* ccl,
					int32* songlength )
{
	init_fat_buffer();

	int1 more_buffers;
	int1 found=0;

	int16 index = highest_i;
	
	int16 closest=fp_dir_entries[highest_i].track;
	*ccl = fp_dir_entries[highest_i].ccl;
	*songlength = fp_dir_entries[highest_i].songlength;		
	
	do
	{			
		int i;

		more_buffers=next_dir_buffer();
		for(i=0;i<fp_dir_nentries  && !found;i++)
		{
			int16 curr_track = fp_dir_entries[i].track;
			if( curr_track == *track )
			{
				*ccl = fp_dir_entries[i].ccl;
				*songlength = fp_dir_entries[i].songlength;
				index = i;
				found=1;
			} else if (curr_track> *track && curr_track < closest)
			{
				closest = curr_track;
				*ccl = fp_dir_entries[i].ccl;
				*songlength = fp_dir_entries[i].songlength;
				index = i;		
			}
		}
	} while( more_buffers && !found);	

	if (!found)
	{
		*track = closest;
	}

	*fp_file_entry = index;

	if( *fp_file_entry>=(fp_dir_nentries-1) && *fp_file_entry!=0 )	
		window_pos = *fp_file_entry-1;
	else
		window_pos = *fp_file_entry;

	return !more_buffers;
}


int1 seek_rand( int32* ccl,
				int32* songlength )
{
	int32 n;
	char page;

	n = (int32)rand();


	int index = (n*stat_nsongs)/RAND_MAX;

	init_fat_buffer();
	end_of_directory=!next_dir_buffer();

	page = 	fp_dir_nentries;
	fp_file_entry=0;

	while( index )
	{	
		page--;
		if(!page)
		{
			page = 	fp_dir_nentries-1;
			fp_file_entry=0;
			end_of_directory=!next_dir_buffer();
		}
		fp_file_entry++;
		index--;
	}

	*ccl = fp_dir_entries[fp_file_entry].ccl;
	*songlength = fp_dir_entries[fp_file_entry].songlength;		
	currentsong = fp_dir_entries[fp_file_entry].track;

	if( fp_file_entry>=(fp_dir_nentries-1) )	
		window_pos = fp_file_entry-1;
	else
		window_pos = fp_file_entry;


	return end_of_directory;
}


void start_program(int i,int1 bell)
{
	alx[i].state = ALARM_STATE_ON;
	currentsong=al[i].track;
	
	state = FM_STATE_ALARM_ON;
	
	// Specific to (M) Variant in order to siable one song playing
	// This allow to specify 00:00 as stop time and not as one song play.
	//player_one_song = al[i].d_mm == 0 && al[i].d_ss==0;	
	player_one_song = 0;
	
	switch(al[i].on)
	{
	case ALARM_OPTION_TRACK:
		player_play_sequential(al[i].track, false);
		break;
	case ALARM_OPTION_RND:
		player_play_random(false);
		break;
	case ALARM_OPTION_BELL_TRACK:
		random_music=0;
		if(bell)
			player_play_bell_ini();
		else
			player_play_sequential(al[i].track, true);
		break;
	case ALARM_OPTION_BELL_RND:
		random_music=1;
		if(bell)
			player_play_bell_ini();
		else
			player_play_random(true);
		break;
	}
	
	lcd_clear();
	lcd_playing_alarm(i);
	lcd_playing_title();
	current_alarm=i;
}

void action_play(int1 repeat) {
	if(!repeat)
	{
	    switch (state) {
        	case FM_STATE_PLAYER:	
				volume=VOLUME_SONG;
				current_ccl = fp_dir_entries[fp_file_entry].ccl;
				current_songlength = fp_dir_entries[fp_file_entry].songlength;		
				currentsong = fp_dir_entries[fp_file_entry].track;
				player_mode = PLAYER_MODE_MUSIC_NO_BELL;
				random_music=0;
				state = FM_STATE_PLAYING;
				play_led = 0;
				break;
			case FM_STATE_PLAYING:
				state = FM_STATE_PAUSED;
				break;
			case FM_STATE_PAUSED:
				state = FM_STATE_PLAYING;
				play_led = 0;
				break;
			break;
        	case FM_STATE_ALARM:
			{
				start_program(cur_alarm,0);
			}	

            break;

		}
	}

}


void stop_alarm(char i)
{
	if( player_mode == PLAYER_MODE_MUSIC_NO_BELL )
	{
		state = FM_STATE_CLOCK;
		lcd_clear();
		lcd_printhour();
		lcd_printready();
		alx[i].state = ALARM_STATE_OFF;
		player_mode = PLAYER_MODE_STOPPED;
	}
	else if( player_mode == PLAYER_MODE_MUSIC_BELL)
	{
		player_mode=PLAYER_MODE_BELL_FIN;
		state = FM_STATE_SKIP;
		lcd_clear();
		lcd_playing_alarm(i);
		lcd_playing_title();
	}

}


void action_stop(int1 repeat) {
	if(!repeat)
	{
	    switch (state) {
        	case FM_STATE_PLAYER:	
        	case FM_STATE_PLAYING:	
        	case FM_STATE_PAUSED:	
				player_mode = PLAYER_MODE_STOPPED;
				state = FM_STATE_PLAYER;
				play_led = 1;
			break;
		}	
	} else {
		if( state == FM_STATE_ALARM_ON )
		{
			stop_alarm(cur_alarm);
			alx[cur_alarm].canceled = true;
		}
	}
}


void timing_blink() {
	
    switch (state) {
        case FM_STATE_CLOCK:
            lcd_blinkcolon(0);
            break;
        case FM_STATE_HSET:	
			if( !isUpOrDown() )
            	lcd_blinkhour(0);
            break;
        case FM_STATE_MSET:
			if( !isUpOrDown() )
            	lcd_blinkminute(0);
            break;
        case FM_STATE_ALARM:
            break;
        case FM_STATE_ALH:
			if( !isUpOrDown() )
        		lcd_blinkalhour(0,cur_alarm);
            break;
        case FM_STATE_ALM:
			if( !isUpOrDown() )
        		lcd_blinkalminute(0,cur_alarm);
            break;
        case FM_STATE_ALT:
			if( !isUpOrDown() )
        		lcd_blinkaltrack(0);
            break;
        case FM_STATE_ALDM:
			if( !isUpOrDown() )
        		lcd_blinkaldminute(0,cur_alarm);
            break;
        case FM_STATE_ALDS:
			if( !isUpOrDown() )
        		lcd_blinkaldsecond(0,cur_alarm);
            break;
        case FM_STATE_ALON:
			if( !isUpOrDown() )
        		lcd_blinkalon(0,cur_alarm);
            break;
    }
}

#inline
void init(void){
	lat_c=0b01000000;
	tris_a=0b01111111;
	tris_b=0b11000101;
	tris_c=0b10010100;//c0 has to be output for mmc transistor...
	tris_d=0b11111111;
	tris_e=0b00000111;//startup NOT in parallel slave port mode
	PLLEN = 1;    // Enable PLL
	ADCON0=0b00000001; //channel 0, ad on
}


#define N_BUTTONS 6
struct button_struct buttons_array[N_BUTTONS];

#define BUTTON_MODE 0
#define BUTTON_SET  1
#define BUTTON_UP  	2
#define BUTTON_DOWN	3
#define BUTTON_PLAY	4
#define BUTTON_STOP	5

int1 isUpOrDown()
{
	return 	(buttons_array[BUTTON_UP].state != BUT_STATE_RELEASED) ||
			(buttons_array[BUTTON_DOWN].state != BUT_STATE_RELEASED);
}

unsigned int1 rdport_mode() { return but_mode; }
unsigned int1 rdport_set() 	{ return but_set; }
unsigned int1 rdport_up() 	{ return but_up; }
unsigned int1 rdport_down() { return but_down; }
unsigned int1 rdport_play() { return but_play; }
unsigned int1 rdport_stop() { return but_stop; }

int oldm=0;

#define INTS_PER_SECOND 256
long int_count;    // Number of interrupts left before a second has elapsed

/* #define INTS_BUTTONS 	2
#define INTS_DATE 		30 */

void reset_rtc()
{

	int i;
    for (i = 0; i < N_ALARMS; i++) {

	    al[i].on = 0;
	    al[i].hh = 0;
	    al[i].mm = 0;
	    al[i].d_mm = 0;
	    al[i].d_ss = 0;
	    al[i].track = 0;
	}
	save_alarms();
	set_hour(0,0,0 );
}

long old_count;

int1 flag_blink=false;
int1 flag_scroll=false;
int1 flag_date=false;
int1 flag_buttons=false;


#define POWER_CONTROL_OFF 0
#define POWER_CONTROL_ON  1


void evaluate_programs(int1 bell)
{
	int alarm_active;
	int1 power_active;
	int i;
	
	flag_date=false;
	
	int temp_hh;
	int temp_mm;
	
	get_date(	&temp_hh, &temp_mm, &sec,
				&dd,&_MM,&yy);
	
	if( state != FM_STATE_HSET &&
		state != FM_STATE_MSET )
	{
		hh = temp_hh;
		mm = temp_mm;
	}
	
	if( oldm!=mm && (state == FM_STATE_CLOCK) )
	{
		lcd_printhour();
		oldm=mm;
	} 
	
	int32 now = 3600LL*hh+60LL*mm+sec;
	
	for( i=0; i<N_ALARMS; i++)
	{
		if( state == FM_STATE_PLAYER ||
			state == FM_STATE_PLAYING  ||
			state == FM_STATE_PAUSED )
		{
			power_control = POWER_CONTROL_ON;
		}
		else if( power_on_enabled )
		{
			if( power_overnight )
			{
				power_active = ((now >= power_on_hour) || (now <= power_off_hour));
			} else {
				power_active = ((now >= power_on_hour) && (now <= power_off_hour));
			}


			if( power_active /* && !power_on_flag */)
			{
				/* power_on_flag = 1; */
				power_control = POWER_CONTROL_ON;
			}
			
			if( !power_active /* && power_on_flag */)
			{
				/* power_on_flag = 0; */
				power_control = POWER_CONTROL_OFF;
			}
		}

		if( alx[i].overnight )
		{
			alarm_active = ((now >= alx[i].start) || (now <= alx[i].end));
		} else {
			alarm_active = ((now >= alx[i].start) && (now <= alx[i].end));
		}

		if( !alarm_active )
		{
			alx[i].canceled = false;
		}

		if( al[i].on &&
			alx[i].state == ALARM_STATE_OFF &&
			alarm_active && !alx[i].canceled &&
			!alarm_being_configured(i))
		{
			alx[i].state = ALARM_STATE_ON;
			currentsong=al[i].track;

			state = FM_STATE_ALARM_ON;

			start_program(i,bell);
		}
					

		if( al[i].on &&
			alx[i].state == ALARM_STATE_ON &&
			!alarm_active &&
			!player_one_song)
		{
				stop_alarm(i);
		}
	}	
}


void check_clock(int1 bell)
{
	int i=0;
	struct t_dir_entry* entry = &fp_dir_entries[fp_file_entry];	

	if( int_count != old_count )
	{
		old_count = int_count;

    	if(flag_blink) {
      		flag_blink=false;
			timing_blink();
			if (state == FM_STATE_PAUSED)
				play_led = ~play_led;
		}

		if( flag_scroll )
		{
			flag_scroll = false;

		    if(  (state == FM_STATE_PLAYER || state == FM_STATE_PLAYING || state == FM_STATE_PAUSED) && 
				get_tit_len(entry)>14 && player_mode!=PLAYER_MODE_BELL_FIN) 
			{
				if (title_delay)
					--title_delay;
				else
				{
					if( fp_file_entry==window_pos )
						lcd_printtrackplnumber(entry,0x81,1,14);
					else
						lcd_printtrackplnumber(entry,0xC1,1,14);
				}
		    } 
		
		    if(  state == FM_STATE_ALT && get_tit_len(entry)>14 
				&& player_mode!=PLAYER_MODE_BELL_FIN) 
			{
				if (title_delay)
					--title_delay;
				else
				{
					lcd_printtrackplnumber(entry,0xC1,1,14);
				}
		    } 
		
		
		    if(  	state == FM_STATE_ALARM_ON && player_mode!=PLAYER_MODE_BELL_INI && player_mode!=PLAYER_MODE_BELL_FIN && 
					get_tit_len(entry)>16 ) 
			{
				if (title_delay)
					--title_delay;
				else
				{
					lcd_printtrackplnumber(entry,0xC0,1,16);
				}
		    } 
		}	
	
	
		if( flag_buttons )
		{

			flag_buttons = false;

			for( i=0; i<N_BUTTONS; i++)
				button_poll(&buttons_array[i]);
			
			if 	( but_mode == 0 &&
				but_up == 0 &&
				but_stop == 0 )
			{
				reset_rtc();
			}

			if ( card_detect == 1 )
			{
				lcd_clear();				
				print_error_no_sd_card();
				while( card_detect == 1 ) {;}
#asm
				RESET
#endasm
			}
		}	
	
	
		if( flag_date )
		{
			evaluate_programs(bell);
		}
    }
}


#int_rtcc                          
void clock_isr() 
{   

	--int_count;

	if(int_count==0) {
    	int_count=INTS_PER_SECOND;
		flag_blink=true;
	}

	//if( (int_count & 127) == 0)
	if( (int_count & 127) == 0)
	{
		flag_scroll=true;
	}

	if( (int_count & 1) == 0 )
	{
		flag_buttons=true;
	}
		
	if ( (int_count & 31) == 0 )
	{
		flag_date=true;
	}

}

#zero_ram

#include "test\\alarms_config.c"

void main(void){

	//unsigned int control_byte;

	int_count=old_count=INTS_PER_SECOND;

    BUT_TRIS |= 0xF0; // Configure buttons
	but_t_mode 	= 1;
	but_t_set 	= 1;
	but_t_up 	= 1;
	but_t_down 	= 1;
	but_t_play 	= 1;
	but_t_stop 	= 1;

	card_detect_t = 1;

	play_led_t = 0;
	play_led = 1;

	power_control_t	= 0;
	power_control	= POWER_CONTROL_OFF;
	
	init();

    lcd_init();

	lcd_boot_screen();

	delay_ms( 1000L ); // Prevents SD Errors at startup.


	if ( card_detect == 1 )
	{
		lcd_clear();				
		print_error_no_sd_card();
		while( card_detect == 1 ) {;}
		delay_ms( 1000L ); // Prevents SD Errors at startup.
	}

	teuthis_init();
	if( error!=0)
	{
		error=0;
		teuthis_init();
	}

	lcd_clear();

	int_count=INTS_PER_SECOND;

	buttons_array[BUTTON_MODE].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_MODE].rdport = rdport_mode;
	buttons_array[BUTTON_MODE].action = action_mode;

	buttons_array[BUTTON_SET].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_SET].rdport = rdport_set;
	buttons_array[BUTTON_SET].action = action_set;

	buttons_array[BUTTON_UP].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_UP].rdport = rdport_up;
	buttons_array[BUTTON_UP].action = action_up;

	buttons_array[BUTTON_DOWN].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_DOWN].rdport = rdport_down;
	buttons_array[BUTTON_DOWN].action = action_down;

	buttons_array[BUTTON_PLAY].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_PLAY].rdport = rdport_play;
	buttons_array[BUTTON_PLAY].action = action_play;

	buttons_array[BUTTON_STOP].state = BUT_STATE_RELEASED;
	buttons_array[BUTTON_STOP].rdport = rdport_stop;
	buttons_array[BUTTON_STOP].action = action_stop;



   	set_timer1(0);
    int i;

	// Uncomment for test alarms recording 
	//save_night_alarms();
	//save_day_alarms();
	//save_fast_alarms();
	//save_night_alarms_orderd();


	get_date(	&hh, &mm, &sec,
				&dd,&_MM,&yy);

	srand(sec*mm);


	lcd_printhour();
	lcd_printready();


	load_alarms();
    for (i = 0; i < N_ALARMS; i++) 
	{
		alx[i].state = ALARM_STATE_OFF;
		alx[i].canceled = 0;
    }

	get_title_stats( 	&stat_nsongs,
						&stat_highest,
						&stat_highest_i );


	init_fat_buffer();
	end_of_directory=!next_dir_buffer();

	if( fp_dir_nentries == 0)
	{
		print_error_no_titles();
		while(1);
	}

   set_timer0(0);
   setup_counters( RTCC_INTERNAL, RTCC_DIV_128 | RTCC_8_BIT);
   enable_interrupts(INT_RTCC);
   enable_interrupts(GLOBAL);


	// If a Restart has been issued in the 
	// middle of a program. The program continues without bell. 
	evaluate_programs(0);

	while(1){		

		check_clock(1);

		if(player_mode != PLAYER_MODE_STOPPED)
		{ 
			
			if( player_mode != PLAYER_MODE_STOPPED )
			{

				if ( player_mode == PLAYER_MODE_BELL_INI ||
					 player_mode == PLAYER_MODE_BELL_FIN )
				{
					volume = VOLUME_BELL;
					song(	cclzero,
						 	songlengthzero/512 );
				} else {
					volume = VOLUME_SONG;
					song(	current_ccl,
						 	current_songlength/512 );
				}
			
				if ( deferred_down )
				{
					deferred_down = 0;
					if ( move_title_down(1))
					{				
						current_ccl = fp_dir_entries[fp_file_entry].ccl;
						current_songlength = fp_dir_entries[fp_file_entry].songlength;		
						currentsong = fp_dir_entries[fp_file_entry].track;
					}
				}

				if( deferred_up )
				{
					deferred_up = 0;
					if( move_title_up(1) )
					{
						current_ccl = fp_dir_entries[fp_file_entry].ccl;
						current_songlength = fp_dir_entries[fp_file_entry].songlength;		
						currentsong = fp_dir_entries[fp_file_entry].track;
					}
				}

	
				if ( state == FM_STATE_SKIP )
					state = FM_STATE_PLAYING;
				else
					player_end_of_song();								
			}

			

		}
	}

}/////////////////////////////////////////end buttons mode///end main() function////////////////////////////////////

