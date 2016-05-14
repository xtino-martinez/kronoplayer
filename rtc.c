/*
 * Delta mp3 player
 * Firmware V0.1 CC http://www.lestmar.com/
 * This software is licensed under Creative Commons 3.0 in the the form of by-nc, wich means 
 * that the use of this code is limited for non-commercial use. Any other use is permitted.
 * The routines used for MMC/SD and mp3 chip are licensed by Raphael Abrahams with different
 * restrictions (please check teuthis.com).
 */

// $Id: rtc.c 56 2010-12-29 10:00:57Z Tino $

#use i2c(master, SDA=PIN_D1, SCL=PIN_D0)
 
#define ds1307_write 0xD0
#define ds1307_read 0xD1

#define pfc8570_write 0xA0
#define pfc8570_read 0xA1


#define USE_RTC 1

static unsigned char get_byte()
{
#ifdef USE_RTC
	unsigned char data;

	i2c_start();
	i2c_write(ds1307_read);
	data = i2c_read(0);
	i2c_stop();

	return data;
#endif

}

static unsigned char get_byte_pfc(char add)
{
#ifdef USE_RTC
	unsigned char data;

	i2c_start();
	i2c_write(pfc8570_write);  
	i2c_write(add);
	i2c_start();
	i2c_write(pfc8570_read);

	data = i2c_read(0);

	i2c_stop();

	return data;
#endif

}

void get_date(	unsigned char *hour,
				unsigned char *min,
				unsigned char *sec,
				unsigned char *day,
				unsigned char *mon,
				unsigned char *year )
{
#ifdef USE_RTC

	unsigned char data;

	i2c_start();
	i2c_write(ds1307_write);  
	i2c_write(0x0);  

	data = get_byte();
	*sec = (data & 0xF) + (data >> 4)*10;
	data = get_byte();
	*min = (data & 0xF) + (data >> 4)*10;
	data = get_byte();
	*hour = (data & 0xF) + ((data >> 4) &0x3)*10;
	data = get_byte(); // TODO: read day of week
	data = get_byte();
	*day = (data & 0xF) + (data >> 4)*10;
	data = get_byte();
	*mon = (data & 0xF) + (data >> 4)*10;
	data = get_byte();
	*year = (data & 0xF) + (data >> 4)*10;

#endif
}

static void set_byte(unsigned char add,unsigned char data)
{
#ifdef USE_RTC
	i2c_start();
	i2c_write(ds1307_write);  
	i2c_write(add);
	i2c_write(data);
	i2c_stop();
#endif
}

static void set_byte_pfc(unsigned char add,unsigned char data)
{
#ifdef USE_RTC
	i2c_start();
	i2c_write(pfc8570_write);  
	i2c_write(add);	
	i2c_write(data);
	i2c_stop();
#endif
}


void set_hour(	unsigned char hour,
				unsigned char min,
				unsigned char sec)
{
#ifdef USE_RTC
	unsigned char data=0;

	data = ((sec%10) & 0xF) | ((sec/10) << 4);
	set_byte(0,data);
	data = ((min%10) & 0xF) | ((min/10) << 4);
	set_byte(1,data);
	data = ((hour%10) & 0xF) | ((hour/10) << 4);
	set_byte(2,data);
#endif
}


void set_date(	unsigned char day,
				unsigned char mon,
				unsigned char year )
{
#ifdef USE_RTC
	unsigned char data=0;

	data = ((day%10) & 0xF) | ((day/10) << 4);
	set_byte(4,data);
	data = ((mon%10) & 0xF) | ((mon/10) << 4);
	set_byte(5,data);
	data = ((year%10) & 0xF) | ((year/10) << 4);
	set_byte(6,data);
#endif 
}


void write_alarm(	int i,
				    unsigned int on,
				    unsigned int hh,
				    unsigned int mm,
				    unsigned int d_mm,
				    unsigned int d_ss,
				    unsigned int track
					)
{
#ifdef USE_RTC

	char mem[8]="01234567";
	write_program_memory(0x7800,mem,8);


/*
	unsigned int add = i*6;
		
	set_byte_pfc(add,on);
	set_byte_pfc(add+1,hh);
	set_byte_pfc(add+2,mm);
	set_byte_pfc(add+3,d_mm);
	set_byte_pfc(add+4,d_ss);
	set_byte_pfc(add+5,track); */
#endif
}

void read_alarm(	int i,
				    unsigned int* on,
				    unsigned int* hh,
				    unsigned int* mm,
				    unsigned int* d_mm,
				    unsigned int* d_ss,
				    unsigned int* track
					)
{

/*
#ifdef USE_RTC
	i2c_start();
	i2c_write(pfc8570_write);  
	i2c_write(i*6);
	i2c_start();
	i2c_write(pfc8570_read);

	*on = i2c_read(1);
	*hh = i2c_read(1);
	*mm = i2c_read(1);
	*d_mm = i2c_read(1);
	*d_ss = i2c_read(1);
	*track = i2c_read(0);	
		
	i2c_stop(); 

#endif
*/
}

void load_calibration( unsigned int* val	)
{
#ifdef USE_RTC
	i2c_start();
	i2c_write(ds1307_write);  
	i2c_write(0x7);  

	*val = get_byte();
#endif
}

void save_calibration( unsigned int val	)
{
#ifdef USE_RTC

	set_byte(7, val);

#endif
}
