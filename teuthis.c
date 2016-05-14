//Daisy mmc mp3
//Firmware V1.2 CC 2001-2006 Raphael Abrams, www.teuthis.com;
//This code is under a Creative Commons license. You may use it for whatever purpose you 
//wish in whatever way you wish, but attribution is required. 
//Please include this paragraph in your source or somewhere easily found by the end user. 
//If there is a printed manual, please make a note somewhere in there too. Like: "mp3 and SD handling routines by teuthis.com" or something like that. 

//The preceding is to be included in any derivative work excepting works falling under "fair use". 
//Consult www.eff.org for more information on Creative Commons.
//Also, while not required, a link to "teuthis.com" would be just the nicest thing! I love free promotion!

// $Id: teuthis.c 62 2011-04-19 00:21:51Z ecelema $


#byte INTCON 	 = 0xFF2
#bit TMR0IF = INTCON.2
#bit INT0IF = INTCON.1

#byte INTCON2 	 = 0xFF1
#bit RBPU=INTCON2.7
#bit	   INTEDG0  = INTCON2.6
#byte INTCON3 	 = 0xFF0

#byte T0CON 	 = 0xFD5

#byte SSPBUF 	 = 0xFC9
#byte SSPADD 	 = 0xFC8
#byte SSPSTAT	 = 0xFC7
#byte SSPCON1	 = 0xFC6
#byte SSPCON2	 = 0xFC5

#byte ADRESH 	 = 0xFC4
#byte ADRESL 	 = 0xFC3
#byte ADCON0 	 = 0xFC2
#bit	   GODONE= ADCON0.1
#byte ADCON1 	 = 0xFC1
#byte ADCON2 	 = 0xFC0


#byte TRIS_C	 = 0xF94
#byte TRIS_B	 = 0xF93

#byte LAT_C	 = 0xF8B
#byte LAT_B	 = 0xF8A

#byte PORT_C	 = 0xF82
#byte PORT_B	 = 0xF81


//spi
#bit  smp =  SSPSTAT.7
#bit  cke =  SSPSTAT.6
#bit  bf =  SSPSTAT.0

#bit  sspen = SSPCON1.5
#bit  ckp = SSPCON1.4
#bit  sspm3 = SSPCON1.3
#bit  sspm2 = SSPCON1.2
#bit  sspm1 = SSPCON1.1
#bit  sspm0 = SSPCON1.0


//a to d
#bit  chs3 = ADCON0.5
#bit  chs2 = ADCON0.4
#bit  chs1 = ADCON0.3
#bit  chs0 = ADCON0.2
#bit  go_done = ADCON0.1  // GO/#DONE   set high, read til low
#bit  adon = ADCON0.0

#bit  vcfg1 = ADCON1.5
#bit  vcfg0 = ADCON1.4
#bit  pcfg3 = ADCON1.3
#bit  pcfg2 = ADCON1.2
#bit  pcfg1 = ADCON1.1
#bit  pcfg0 = ADCON1.0

#bit  adfm = ADCON2.7 //sets left or right justification for 12 bit result


#bit MMCSS	= lat_c.1 //this is chip select for the MMC/SD
#bit XCS		= lat_b.4
#bit XDCS		=lat_b.3
#bit XCLK_l		= lat_c.3
#bit XCLK_p		= port_c.3
#bit XCLK_TRIS= tris_c.3
#bit XDI_p		= port_c.4//this is the tricky one that goes both ways, software spi and hardware spi
#bit XDI_l		= lat_c.4//this is the tricky one that goes both ways, software spi and hardware spi
#bit XDO	 	= port_c.0//not used, most likely
#bit MMC_TRANSISTOR= lat_c.0
#bit XDREQ		= port_b.2
#bit XRESET 	= lat_b.1

#bit XDI_TRIS	=tris_c.4


#bit 	but_t_play	= tris_c.2
#bit 	but_play	= port_c.2

#bit 	card_detect_t	= tris_c.7
#bit 	card_detect		= port_c.7

#bit 	play_led_t		= tris_b.5
#bit 	play_led		= lat_b.5

#bit 	power_control_t	= tris_c.6
#bit 	power_control	= port_c.6

char data_lo, data_hi;


char BPB_SecPerClus;
int16 bpbstart;
int16 FirstDataSector;
int16 BPB_ResvdSecCnt;


int32 fatstart, datsec;

int32 BPB_FATSz32;
int32 BPB_RootClus;
char BPB_NumFATs;

int32 root_ccl;
char throttle;
int16 BPB_BytsPerSec;
//#define volume_left 0xdf
//#define volume_right 0xdf

/*
#define volume_left 0xff
#define volume_right 0xff
*/

int volume = 0xff;

#define mmc_high_speed 0b00100000
#define mmc_med_speed 0b00100001
#define mmc_low_speed 0b00100010


///////////////////prototypes
char rand(void);
#inline
void init(void);

void  find_bpb(void);
void printout_rootdir(void);
int32 readfat(int32 fatoffset);
#inline
int32 numbergetter(int16 name, int32* closest);
void get_track_bounds(int16* firstTrack, int16* lastTrack);
void find_highest_song_number(void);
int16 true_random(int1 clip);

char  getsongstart(int32 filenumber, int1 kludge);
char  song(int32 startcluster,int32 totalsectors);
void play_range(int32 start, int32 end);
void init_fat_buffer();
int next_dir_buffer();

void serialhandler(void);
void serialhandler_full(void);

int mmc_init(int1 report);


#inline
int mmc_response(unsigned char response);
#inline
int mmc_response_masked(unsigned char mask);

void  mmc_skip(char count);
void mmc_read(void);
int mmc_get_status();
char mmc_open_block(int32 block_number);
char mmc_get_byte(void);
void mmc_close_block(void);
void mmc_cancel_block(void);

int mmc_read_block_to_vs1011(int32 block_number);

int16 vs_command(char inout,address,a,b);
#inline
void resetvs1011_hard(void);
void resetvs1011_soft(void);
void morezeroes(int1 halted);
char vs_spi_write(char aa);

///////////////////////end prototypes

struct t_dir_entry {
	int16 track;
	int32 ccl;
	int32 songlength;
	char title[32];
	int1 tit_full;
	char tit_p;
	char tit_offset;
};

#define MAX_DIR_ENTRY 6

struct t_dir_entry fp_dir_entries[MAX_DIR_ENTRY+1];
char fp_dir_nentries;
int32 fp_cur_cluster;
char fp_cur_sector;
char fp_sec_offset;
int16 fp_dir_global_entries;

unsigned char curr_window;

int32 cclzero=0;
int32 songlengthzero=0;

void get_title_stats( 	int16* nsongs,
						int16* highest,
						int16* highest_i )
{
	int1 first=1;

	*nsongs=0;

	*highest=0;
	init_fat_buffer();

	int1 more_buffers;
	
	do
	{			
		int i;
		more_buffers=next_dir_buffer();
		
		if( first)
		{
			*nsongs += 	fp_dir_nentries;
			first=0;
		} else {
			*nsongs += 	fp_dir_nentries-1;
		}

		for(i=0;i<fp_dir_nentries;i++)
		{	
			if( fp_dir_entries[i].track > *highest )
			{
				*highest = fp_dir_entries[i].track;			
				highest_i = i;
			}
		}
	} while( more_buffers );
}


char get_tit_len(struct t_dir_entry* entry)
{
	if( entry->tit_full )
		return 32;
	else
		return 32-entry->tit_p;
}

void init_fat_buffer()
{
	root_ccl= BPB_RootClus;
	fp_cur_cluster=root_ccl;
	fp_cur_sector=0;
	fp_sec_offset=0;
	curr_window=0;
	fp_dir_global_entries=0;
}


void reset_title(struct t_dir_entry* entry)
{
	entry->tit_full=0;
	entry->tit_p=31;
}


void get_long_name_entry(struct t_dir_entry* entry,char* data)
{
	signed int i;

	if( (data[0] & 0xE0) == 0x40)
	{
		entry->tit_full=0;
		entry->tit_p=31;
	}
	
	for(i = 30; i>=28; i-=2)
		if( data[i]!= 0 &&  data[i]!= 0xff)
		{
			entry->title[entry->tit_p]=data[i];
			if (entry->tit_p==0)
			{
				entry->tit_p=31;
				entry->tit_full=1;
			} else
				entry->tit_p--;
		}
	
	
	for(i=24; i>=14; i-=2)
		if( data[i]!= 0 &&  data[i]!= 0xff)
		{
			entry->title[entry->tit_p]=data[i];
			if (entry->tit_p==0)
			{
				entry->tit_p=31;
				entry->tit_full=1;
			} else
				entry->tit_p--;
		}
	
	for( i=9; i>=1; i-=2)
		if( data[i]!= 0 &&  data[i]!= 0xff)
		{
			entry->title[entry->tit_p]=data[i];
			if (entry->tit_p==0)
			{
				entry->tit_p=31;
				entry->tit_full=1;
			} else
				entry->tit_p--;
		} 													

}


void get_short_name_entry(struct t_dir_entry* entry,char* data)
{
	signed int j=7;							
	entry->title[31] = data[10];
	entry->title[30] = data[9];
	entry->title[29] = data[8];
	entry->title[28] = '.';
	entry->tit_p= 27;
	while( data[j]==' ' && j>0) j--;
	for(;j>=0;j--)
	{
		entry->title[entry->tit_p] = data[j];
		entry->tit_p--;
	}													
}

int next_dir_buffer()
{
	unsigned char data[32];
	int32 ccltemp, eocmark;;

	++curr_window;

	if(curr_window==1)
	{
		fp_dir_nentries=0;
		reset_title(&fp_dir_entries[0]);
	} else {
		fp_dir_entries[0] = fp_dir_entries[MAX_DIR_ENTRY];
		fp_dir_nentries=1;
		reset_title(&fp_dir_entries[1]);
	}

	do {		

		if (fp_cur_sector==BPB_SecPerClus)
		{
			// Going to next cluster
			fp_cur_cluster=readfat(fp_cur_cluster);
			fp_cur_sector=0;
		}
	
		eocmark=fp_cur_cluster & 0x0FFFFFFF;		

		if( eocmark<0x0ffffff0 )
		{

			ccltemp=fp_cur_cluster-2;
			ccltemp=ccltemp * (int32)BPB_SecPerClus;
			ccltemp=ccltemp + (int32)datsec;
			ccltemp=ccltemp + (int32)fp_cur_sector;
			if (mmc_open_block(ccltemp) != 0)
			{
				//LED=0;
				error=8;
			}

		 	mmc_skip(fp_sec_offset);
		
			do {				

				int32 ccl;

				mmc_read();
				if((data_lo==0x00)||(data_lo == 0xe5)){//these are either blank entries or deleted ones. probably should escape from the loop if they are blank, but just to be sure we'll look to the end.
					mmc_skip(15); 
				} else {

		
					signed int i;
					char Emm,Pee,Three;
					int32 Largeccl /*, ccltemp */;
					struct t_dir_entry* entry = &fp_dir_entries[fp_dir_nentries];

					for(i=0;i<30;)
					{
						data[i]=data_lo;
						i++;
						data[i]=data_hi;
						i++;
						mmc_read();
					}
					data[i]=data_lo;
					data[i+1]=data_hi;
				
					if ( (data[11] & 0xFCU) == 0x20  && 
							data[0] >= '0' && data[0] <= '9' &&
							data[1] >= '0' && data[1] <= '9' &&
							data[2] >= '0' && data[2] <= '9' &&
							data[3] >= '0' && data[3] <= '9' )
					{
						int indexs[4];
						int16 track;

						for(i=0;i<4;i++)
						{
							indexs[i] = data[i] - 48;
						}

						track = indexs[0]*1000 + indexs[1]*100 + indexs[2]*10 + indexs[3];

							Emm=	data[8];		//Emm=data_lo;
							Pee=	data[9];		//Pee=data_hi;
							Three=	data[10];		//Three=data_lo;
		
							if (	((Emm=='m') || (Emm=='M')|| (Emm=='W')|| (Emm=='w')) && 
									((Pee=='P') || (Pee=='p')|| (Pee=='A')|| (Pee=='a')) && 
									((Three=='3')||(Three=='V')||(Three=='v')) )
							{
								int32 songlength;
								Largeccl= ((int16)data[21]<<8)+data[20];
								Largeccl=Largeccl<<16;
								ccl= ((int32)data[27]<<8)+data[26];


								*(((char*)&songlength)+0)=data[28];
					 			*(((char*)&songlength)+1)=data[29];
								*(((char*)&songlength)+2)=data[30];
								*(((char*)&songlength)+3)=data[31];

								if( track > 0)
								{

									fp_dir_entries[fp_dir_nentries].ccl = ccl+ Largeccl;   //add on the high bytes of the cluster address

									fp_dir_entries[fp_dir_nentries].songlength = songlength;
									fp_dir_entries[fp_dir_nentries].track = track;
			
									entry->tit_offset = 0;
		
									entry->title[entry->tit_p]=' ';
		
									// Entry in 8.3 format
									if( entry->tit_p==31 && !entry->tit_full)
									{	
										get_short_name_entry(entry,data);
									}
		
									
									++fp_dir_nentries;
									++fp_dir_global_entries;
		
									if( fp_dir_nentries < (MAX_DIR_ENTRY+1))
										reset_title(&fp_dir_entries[fp_dir_nentries]);
								
								} else {

									cclzero = ccl+ Largeccl;   //add on the high bytes of the cluster address
									songlengthzero = songlength;

								}
						}
					}  else if ( data[11] == 0x0F )
					{
						get_long_name_entry(entry,data);
					} 
				}
				fp_sec_offset += 16;
			} while ( fp_sec_offset!=0 && fp_dir_nentries<(MAX_DIR_ENTRY+1) );
	
			if( fp_sec_offset == 0 )
			{
				++fp_cur_sector;
			}			

			mmc_skip(256-fp_sec_offset);
			mmc_close_block();
		}

	}while(eocmark<0x0ffffff0 &&  fp_dir_nentries<(MAX_DIR_ENTRY+1)); //	printf("file does not exist!");putc(13);putc(10);	

	delay_cycles(1);

	return (eocmark<0x0ffffff0);
}

void prev_dir_buffer()
{
	int search_window = curr_window-1;

	init_fat_buffer();
	if( search_window == 0 )
		while(next_dir_buffer());
	else
		while(next_dir_buffer() && curr_window!=search_window);
}



void teuthis_init()
{
	char result, n;

	ADCON1=0b00001111;
	throttle=mmc_low_speed;

	ADCON1=0b00001010;//this must be AFTER the lines above, or else the jumper pins will be analog at the wrong time
	ADCON2=0b10001000;
	INTEDG0=0;
	RBPU=0;
	MMC_TRANSISTOR=1;//for p-channel, this would be off
	xreset=0;//hard reset
	delay_ms(50);

	resetvs1011_hard();
	xcs=1;
	xdcs=1;

	result=1;
	for (n=0;(result==1)&(n<10);n++){
		result=mmc_init(1);
	}

	/*
	if (result==1){
		//printf("problem initializing card, going to sleep"); while(1){sleep();} 
		error=1;
	} */

	find_bpb();
	if(BPB_BytsPerSec!=0x200){
		//printf("problem reading BPB");	putc(13);putc(10);
		find_bpb();
		error=2;
	}
	//printf("file system found");	putc(13);putc(10);
	//find_highest_song_number();

	root_ccl= BPB_RootClus;	
	
}


char	song(int32 startcluster,int32 totalsectors) {
	int32 ccltemp,eocmark,totalsectorsdone;

	char x;
	
	enable_sd_check=0;

	x=0;
	totalsectorsdone=0;
	vs_command(0x02,0x0b,254,254);

	resetvs1011_soft();
	vs_command(0x02,0x0b,255-volume,255-volume);
	//startsector	=	startsector/BPB_SecPerClus;
	//endsector	=	endsector/BPB_SecPerClus;
	//enable_interrupts(INT_TIMER0);



	do{
		for (x=0;x<BPB_SecPerClus && player_mode != PLAYER_MODE_STOPPED && state != FM_STATE_SKIP;x++){
			totalsectorsdone++;
			ccltemp=startcluster-2;
			ccltemp=ccltemp * (int32)BPB_SecPerClus;
			ccltemp=ccltemp + (int32)datsec;
			ccltemp=ccltemp + (int32)x;
			if(totalsectorsdone<totalsectors){
				if(mmc_read_block_to_vs1011(ccltemp)==1){;//this function call is where data goes to the decoder


					if(mmc_read_block_to_vs1011(ccltemp)==1){

							if(mmc_read_block_to_vs1011(ccltemp)==1){
								//printf("lost a sector!");
								player_mode = PLAYER_MODE_STOPPED;
							}

					}
				}

				while( state == FM_STATE_PAUSED) check_clock(1);
			}
	
		}

		//if(startsector<endsector+10){//+10 for safety, can be taken out if it proves to be irrelevant
			startcluster=readfat(startcluster); 
		//}
		vs_command(0x02,0x0b,255-volume,255-volume);

		eocmark=startcluster & 0x0FFFFFFF;
	}while(eocmark<0x0fffffef && player_mode != PLAYER_MODE_STOPPED && state != FM_STATE_SKIP );

	enable_sd_check=1;

	return('s');
}


////////////////////////////////////////////////////////////////////////////////////////////////
void play_range(int32 start, int32 end){
	for(;start<end;start++){
		mmc_read_block_to_vs1011(start);
	}
}




///////////// ///////////// ////////////////// ///////////????????????//////////////
void find_bpb(){
int32 bigtemp;
	bpbstart=0;
	mmc_open_block(0);
	mmc_skip(0xE3);
	mmc_read();
	bpbstart=((int16) data_hi<<8);
	bpbstart+=data_lo;
//	mmc_cancel_block();
	mmc_skip(27);
//	mmc_skip(35);
	mmc_close_block();
	mmc_open_block((int32) bpbstart);

	mmc_skip(5);
	mmc_read();
	BPB_bytspersec=data_hi;	//11
	mmc_read();
	BPB_bytspersec=BPB_bytspersec+((int16) data_lo<<8); //11.12
	BPB_SecPerClus=data_hi;	//13
	mmc_read();
	BPB_ResvdSecCnt=((int16) data_hi<<8)+data_lo;  //14.15
	mmc_read();

	BPB_NumFATs=data_lo;  //16      //BPB_RootEntCnt=data_hi;  //17
	mmc_read();//	BPB_RootEntCnt=BPB_RootEntCnt+((int16) data_lo<<8);  //17.18
	mmc_read();
	mmc_read();//	BPB_FATSz16=data_lo+((int16) data_hi<<8);  //22.23

	mmc_skip(6);
	mmc_read();
	BPB_FATSz32=0;
	BPB_FATSz32=data_lo+((int32) data_hi<<8);  //36.37
	bigtemp=BPB_FATSz32;
	mmc_read();
	BPB_FATSz32=data_lo+((int32) data_hi<<8);  //36.37.38.39
	BPB_FATSz32=BPB_FATSz32 <<16;
	BPB_FATSz32=bigtemp+BPB_FATSz32;
	mmc_read();
	mmc_read();
	mmc_read();
	BPB_RootClus=data_lo+((int32) data_hi<<8);  //44.45
	bigtemp=BPB_RootClus;

	mmc_read();
	mmc_skip(231);
//	mmc_cancel_block();
	mmc_close_block();
	BPB_RootClus=data_lo+((int32) data_hi<<8);  //44.45.46.47
	BPB_RootClus=BPB_RootClus<<16;
	BPB_RootClus=BPB_RootClus+bigtemp;
	root_ccl=BPB_RootClus;

	fatstart=bpbstart+BPB_ResvdSecCnt;
	FirstDataSector = BPB_ResvdSecCnt + (BPB_NumFATs * BPB_FATSz32);// + RootDirSectors;
	datsec = (int32)bpbstart + (int32)firstdatasector;
/*
	putc(13);putc(10);
	printf("some info about card:"); putc(13);putc(10);
	printf(" bpbst:%4LX  ",bpbstart);						putc(13);putc(10); 
	printf(" BPB_bpsec:%4LX  ",BPB_bytspersec);			putc(13);putc(10); 
	printf(" BPB_ScPeC:%4LX  ",BPB_SecPerClus);		putc(13);putc(10); 
	printf(" BPB_RsvdScCt:%4LX  ",BPB_ResvdSecCnt);	putc(13);putc(10); 
	printf(" BPB_NmFAT:%4LX  ",BPB_NumFATs);		putc(13);putc(10); 
	printf(" fatstart: %4LX  ",fatstart);						putc(13);putc(10); 
	printf(" FirstDataSector:%4LX  ",FirstDataSector);		putc(13);putc(10);
*/
}


int32 readfat(int32 fatoffset){
int16 temp;
int32 tempb;
char los;

	temp=0;
	fatoffset=fatoffset*2;
	los = *(((char*)&fatoffset)+0);	//the bottom byte of the address goes directly to a word in the FAT
	fatoffset=fatoffset / 256; 
	fatoffset+=fatstart;
	if(mmc_open_block(fatoffset)==1){
		//putc('^');
//		printf("fat retry...");
		if(mmc_open_block(fatoffset)==1){
			//putc('&');
			mmc_init(0);
//			printf("fat RETRY...");
			if(mmc_open_block(fatoffset)==1){
				//putc('*');
				mmc_init(0);
//				printf("fat LOOKS BAD...");
				if(mmc_open_block(fatoffset)==1){
					//printf("problem reading fat table, quitting chain. Sorry!");
					error=7;
					return 0xffffffff;
				}
			}
		}
	}
	mmc_skip(los);
	mmc_read();
	temp = ((int16) data_hi * 256)+ (int16) data_lo;
	mmc_read();//for fat32, four bytes per entry
	tempb=0;
	tempb = ((int16) data_hi * 256)+ (int16) data_lo;
	tempb=tempb<<16;
	tempb=tempb+(int32) temp;
	mmc_skip(255-(los));//trouble???
	mmc_read();
	mmc_read();
	mmc_close_block();
	return tempb;
}
//-----------------------------------


////////////////////////////////////////////////////////////////////////////////////////////////

void resetvs1011_hard(void){
	MMCSS=1;	xcs=1;	xdcs=1;
	SSPEN=0;
	delay_ms(50);
		do{;
			xreset=0;//hard reset
			delay_us(250);
			xreset=1;
			delay_ms(50);
			delay_ms(50);
		}while(!XDREQ);
		do{;
		delay_cycles(500);
		vs_command(0x02,0x00,0x00,0x04);
		delay_ms(10);
		}while(!XDREQ);
		do{;
			delay_ms(50);
			delay_ms(50);
		}while(!XDREQ);
	delay_ms(1);
	vs_command(0x02,0x00,0x08,0x00);//go into new mode (w/o pin sharing, w/o sdi tests)
	vs_command(0x02,0x0b,255-volume,255-volume);//set volume
	delay_ms(1);
//		puts("VS1011: hard reset");
}


//-----------------------------------
void resetvs1011_soft(void){
	vs_command(0x02,0x00,0x08,0x04);//go into new mode (w/o pin sharing, w/o sdi tests)
	delay_ms(1);
	vs_command(0x02,0x0b,255-volume,255-volume);//set volume
	delay_ms(1);
	delay_ms(1);
}
///////////////////////////////////////////////////////////////////////////////////////////////
void morezeroes(int1 halted){
int16 c;
	xdcs=0;
	for(c=4096;c>0;c--){
		vs_spi_write(0);
		while ((halted)&(!XDREQ)){;}
	}
	xdcs=1;
}
////////////////////////////////////////////////////////////////////////////////////////////////
char vs_spi_write(char aa){
char h;
char i;
	h=0;
	i=7;
	XCLK_l=0;delay_cycles(10);
	do{ 
		XDI_l=bit_test(aa,i);	
		XCLK_l=1;
		delay_us(10);
		if(XDO==1){bit_set(h,i);}	
		XCLK_l=0;
		delay_us(10);
	}while((i--)>0);
	return (h);
}
////////////////////////////////////////////////////////////////////////////////////////////////
//#inline
int16 vs_command(char inout, address,a,b){//for sending non-song related data to the decoder..
int16 result;
	SSPCON1 = 0x00; //sspconkludge?
	MMCSS=1;
	xdcs=1;
	XDI_TRIS=0;
	xcs=0;
	XCLK_TRIS=0;
	vs_spi_write(inout); 
	vs_spi_write(address);//load the buffer with address
	*(((char*)&result)+1)=vs_spi_write(a);			//load buffer with hi  data(ignored on read command)
	*(((char*)&result)+0)=vs_spi_write(b);			//load buffer with data low value
	xcs=1;				//deselect the vs1011 command registers (go into sdi mode)
	XDI_TRIS=1;
	SSPCON1=throttle;//used to be 0b00100001
	return (result);
}
////////////////////////////////////////////////////////////////////////////////////////////////
void mmc_read(void){
	data_lo=SPI_READ(0xFF);
	data_hi=SPI_READ(0xFF);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void mmc_skip(char count){
	for (;count>0;count--){
		SPI_WRITE(0xFF);
		SPI_WRITE(0xFF);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////
char mmc_open_block(int32 block_number){
char tries,i;
int32 block_number_temp;

	for(tries=0;tries<10;tries++){
		SSPCON1=throttle;
		block_number_temp=block_number*512;
		SPI_WRITE(0xFF);
		SPI_WRITE(0xFF);
		MMCSS=0;                     // set MMCSS = 0 (on)
		SPI_WRITE(0x51);                // send mmc read single block command
		SPI_WRITE(*(((char*)&block_number_temp)+3)); // arguments are address
		SPI_WRITE(*(((char*)&block_number_temp)+2));
		SPI_WRITE(*(((char*)&block_number_temp)+1));
		SPI_WRITE(0x00);
		SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
		if((mmc_response(0x00))==0){//((mmc_response(0x00))==0){
			if((mmc_response(0xFE))==0){
				return(0);	
			}

			for(i=0;i<255;i++){
				SPI_WRITE(0xFF);
				SPI_WRITE(0xFF);
			}
		}
		MMCSS=1;            // set MMCSS = 1 (off)
		SPI_WRITE(0xFF);// give mmc the clocks it needs to finish off
		SPI_WRITE(0xFF);// give mmc the clocks it needs to finish off
	}

	return 1;
}

void mmc_close_block(void){

	SPI_READ(0xFF);                 // CRC bytes that are not needed
	SPI_READ(0xFF);
	MMCSS=1;            // set MMCSS = 1 (off)
	SPI_WRITE(0xFF);                // give mmc the clocks it needs to finish off
       SPI_WRITE(0xff);
}


void mmc_cancel_block(void){
	mmcss=1;
	SPI_WRITE(0xFF);	
	SPI_WRITE(0xFF);
	mmcss=0;
	SPI_WRITE(0xFF);
	SPI_WRITE(0x4C);                // send mmc cancel block command
	SPI_WRITE(0); //no arguments
	SPI_WRITE(0);
	SPI_WRITE(0);
	SPI_WRITE(0);
	SPI_WRITE(0xFF);
	mmc_response(0x00);
		MMCSS=1;
		SPI_WRITE(0xFF); //	printf(" MMC block cancelled");	putc(13);putc(10);
	       SPI_WRITE(0xff);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
int mmc_init(int1 report){	//Initialises the MMC into SPI mode and sets block size
//char p;
int i, ii, tries;

for(tries=0;tries<10;tries++){
	MMC_TRANSISTOR=1;
	XCLK_l=0;
	delay_ms(30);	
	SSPSTAT |= 0x40;                          // set CKE = 1 - clock idle low
	SSPCON1=mmc_low_speed; //was 0b00100001
	MMCSS=1;			//                    // set MMCSS = 1 (off)
	MMC_TRANSISTOR=0;
	delay_ms(30);
	for(i=0;i<10;i++){                       // initialise the MMC card into SPI mode by sending clks on
	        SPI_WRITE(0xFF);
	}
	MMCSS=0;				                     // set MMCSS = 0 (on) tells card to go to spi mode when it receives reset
	SPI_WRITE(0x40);                        // send reset command
 	SPI_WRITE(0x00);
 	SPI_WRITE(0x00);
 	SPI_WRITE(0x00);
 	SPI_WRITE(0x00);
	SPI_WRITE(0x95);                        // precalculated checksum as we are still in MMC mode
/*
	if(report){
		puts(" \n\r");
		puts("MMC/SD init: Sent 0x40 (cmd0) SPI\n\r");
	}  */
	if(mmc_response(0x01)==0){// return 1;     // if = 1 then there was a timeout waiting for 0x01 from the mmc (bad!)
		/*
		if(report){
			puts("MMC/SD init: Got response \n\r");
		} */
		i = 0;
		ii=0;
		do{     // must keep sending command if response
			MMCSS=1;
	
		       SPI_WRITE(0xff);
		       SPI_WRITE(0xff);
	
			MMCSS=0;
			SPI_WRITE(0x77);//command 55                // send mmc command one to bring out of idle state
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
			SPI_WRITE(0xF0);                // checksum is no longer required but we always send 0xFF
			while(mmc_response_masked(0b10000000)==1){
				i++;
				if(i>10){
					ii = 0;
					do{
						MMCSS=1;
		       			SPI_WRITE(0xff);
					       SPI_WRITE(0xff);
						MMCSS=0;
						SPI_WRITE(0x41);                // send mmc command one to bring out of idle state
					 	SPI_WRITE(0x00);
					 	SPI_WRITE(0x00);
					 	SPI_WRITE(0x00);
					 	SPI_WRITE(0x00);
					       SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
	      			 		ii++;
						/*
						if(report){
							printf(".%u",ii);
						} */
						if (mmc_response_masked(0b10000001)==0){
							/*
							if(report){
								printf("yes, it's an mmc!");putc(13);putc(10);
							} */
							goto jump;
						}
					}while(ii < 255);     // must keep sending command if response
				}
			}	
	
		       SPI_WRITE(0xff);
		       SPI_WRITE(0xff);
		       SPI_WRITE(0xff);
		       SPI_WRITE(0xff);
	
			SPI_WRITE(0x69);//command 41, not command 1                // send mmc command one to bring out of idle state
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
		 	SPI_WRITE(0x00);
			SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
		       i++;
			/*
			if(report){	
				printf("...%ud ",i);// putc(13);putc(10);
			} */
			if (mmc_response_masked(0b10000001)==0){goto jump;}
		}while(i < 255);//we want a zero here, right?
		/*
		if(report){
			printf("failed out of idle---");putc(13);putc(10);
		} */
		error=3;
		return 1;
	
jump:
		/*
		if(report){
			putc(13);putc(10);
			puts("MMC/SD init: Got out of idle response \n\r");putc(13);putc(10);
		} */
		MMCSS=1;                    // set MMCSS = 1 (off)
		SPI_WRITE(0xFF);                        // extra clocks to allow mmc to finish off what it is doing
	       SPI_WRITE(0xff);
	       SPI_WRITE(0xff);
	       SPI_WRITE(0xff);
		MMCSS=0;                     // set MMCSS = 0 (on)
	       SPI_WRITE(0xff);
		SPI_WRITE(0x50);                // block size command
		SPI_WRITE(0x00);
		SPI_WRITE(0x00);
		SPI_WRITE(0x02);                // high block length bits - 512 bytes
		SPI_WRITE(0x00);                // low block length bits
		SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
		if((mmc_response(0x00))==1) {  error=4; return 1; //bad! 
									}
		SPI_WRITE(0xff);                // 
		SPI_WRITE(0xff);                // 
		MMCSS=1;            //off
		/*
		if(report){
			puts("MMC/SD init: Got set block length response. Done.\n\r");putc(13);putc(10);
		} */	
		SPI_WRITE(0xff);                // 
		SPI_WRITE(0xff);                // 
		sspcon1=throttle;
		return 0; //good
	}
}
}
	



////////////////////////////////////////////////////////////////////////////////////////////////
int mmc_get_status(){	// Get the status register of the MMC, for debugging
char ret=0;
char  p;
	xcs=1;
	xdcs=1;
	MMCSS=0;                     // set MMCSS = 0 (on)
	SPI_WRITE(0x7a);                // 0x58?
	for(p=4;p>0;p--){
		SPI_WRITE(0x00);
	}
	SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
	if( mmc_response(0x00) == 0 &&
		mmc_response(0xff) == 0) ret=1;
	MMCSS=1;                    // set MMCSS = 1 (off)
	SPI_WRITE(0xFF);
	SPI_WRITE(0xFF);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////
#inline
int mmc_response(unsigned char response){	//reads the MMC until we get the response we want or timeout
int16 count;        		// 16bit repeat, it may be possible to shrink this to 8 bit but there is not much point
char temp;
	for(count=520;count>0;count--){
		temp = SPI_READ(0xFF);
		if(temp == response){
			return (0);
		}
	}
	return 1;
}


#inline
int mmc_response_masked(unsigned char mask){	//reads the MMC until we get the response we want or timeout
int16 count;        		// 16bit repeat, it may be possible to shrink this to 8 bit but there is not much point
char temp;
	for(count=520;count>0;count--){
		temp = SPI_READ(0xFF);
		temp =temp & mask;
		if(temp==0){
			return (0);
		}
	}
	return 1;
}


////////////////////////////////////////////////////////////////////////////////////////////////
#define KLEE 32

//#inline
int mmc_read_block_to_vs1011(int32 block_number){
unsigned long i, ii;//, j;

	SSPCON1=throttle;//used to be 0b00100001

	block_number*=2;
		SPI_WRITE(0xFF);
		SPI_WRITE(0xFF);
		MMCSS=0;                     // set MMCSS = 0 (on)
	SPI_WRITE(0x51);                // send mmc read single block command
	SPI_WRITE(*(((char*)&block_number)+2)); // arguments are address
	SPI_WRITE(*(((char*)&block_number)+1));
	SPI_WRITE(*(((char*)&block_number)+0));
	SPI_WRITE(0x00);
	SPI_WRITE(0xFF);                // checksum is no longer required but we always send 0xFF
	
	if((mmc_response(0x00))==0) {
		if((mmc_response(0xFE))==0){
			XDCS=0;//allow vs1011 to see the spi data

			for(i=16;i>0;i--){
				while(!XDREQ){ ; }
				//led=LEDON;
				for(ii=0;ii<32;ii++){
				SPI_write(0xff);
				check_clock(1); 
				}
				//led=LEDOFF;
			}
			XdCS=1;//turn off vs1011 chip select
somere:
			SSPCON1=throttle;//used to be 0b00100001	SETUP_SPI(SPI_MASTER | SPI_H_TO_L | SPI_CLK_DIV_4 | SPI_SS_DISABLED);
			SPI_READ(0xFF);                 // CRC bytes that are not needed, so we just use 0xFF
			SPI_READ(0xFF);
			MMCSS=1;            // set MMCSS = 1 (off)
			SPI_WRITE(0xFF);// give mmc the clocks it needs to finish off
			SPI_WRITE(0xFF);
			return 0;
		}
		SPI_WRITE(0xFF);
		SPI_WRITE(0xFF);
		MMCSS=1;            // set MMCSS = 1 (off)
		SPI_WRITE(0xFF);// give mmc the clocks it needs to finish off
		SPI_WRITE(0xFF);
		return 1;
	}
	SPI_WRITE(0xFF);
	SPI_WRITE(0xFF);
	MMCSS=1;            // set MMCSS = 1 (off)
	SPI_WRITE(0xFF);// give mmc the clocks it needs to finish off
	SPI_WRITE(0xFF);
	return 1;
}


