/*
 * wwsr - Wireless Weather Station Reader
 * 2007 dec 19, Michael Pendec (michael.pendec@gmail.com)
 * Version 0.5
 * 2008 jan 24 Svend Skafte (svend@skafte.net)
 * 2008 sep 28 Adam Pribyl (covex@lowlevel.cz)
 * Modifications for different firmware version(?)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <usb.h>

struct usb_dev_handle *devh;
int	ret,mempos=0,showall=0,shownone=0,resetws=0,pdebug=0,postprocess=0;
int	o1,o2,o3,o4,o5,o6,o7,o8,o9,o10,o11,o12,o13,o14,o15;
char	buf[1000],*endptr;
char	buf2[400];

void _close_readw() {
    ret = usb_release_interface(devh, 0);
    if (ret!=0) printf("could not release interface: %d\n", ret);
    ret = usb_close(devh);
    if (ret!=0) printf("Error closing interface: %d\n", ret);
}

struct usb_device *find_device(int vendor, int product) {
    struct usb_bus *bus;
    
    for (bus = usb_get_busses(); bus; bus = bus->next) {
	struct usb_device *dev;
	
	for (dev = bus->devices; dev; dev = dev->next) {
	    if (dev->descriptor.idVendor == vendor
		&& dev->descriptor.idProduct == product)
		return dev;
	}
    }
    return NULL;
}

struct tempstat {
	char ebuf[271];
	unsigned short  noffset;
	char delay1;
	char hindoor;
	signed int tindoor;
	unsigned char houtdoor;
	signed int toutdoor;
	unsigned char swind;
	unsigned char swind2;
	unsigned char tempf;
	int pressure;
	unsigned char temph;
	unsigned char tempi;
	signed int rain;
	signed int rain2;
	unsigned char rain1;
	unsigned char oth1;
	unsigned char oth2;
	char nbuf[250];
	char winddirection[100];
} buf4;


void print_bytes(char *bytes, int len) {
    int i;
    if (len > 0) {
	for (i=0; i<len; i++) {
	    printf("%02x ", (int)((unsigned char)bytes[i]));
	}
	// printf("\"");
    }
}
void _open_readw() {
    struct usb_device *dev;
    int vendor, product;
#if 0
    usb_urb *isourb;
    struct timeval isotv;
    char isobuf[32768];
#endif

    usb_init();
//    usb_set_debug(0);
    usb_find_busses();
    usb_find_devices();

    vendor = 0x1941;
    product = 0x8021; 

    dev = find_device(vendor, product);
    assert(dev);
    devh = usb_open(dev);
    assert(devh);
    signal(SIGTERM, _close_readw);
    ret = usb_get_driver_np(devh, 0, buf, sizeof(buf));
    if (ret == 0) {
	// printf("interface 0 already claimed by driver \"%s\", attempting to detach it\n", buf);
	ret = usb_detach_kernel_driver_np(devh, 0);
	// printf("usb_detach_kernel_driver_np returned %d\n", ret);
    }
    ret = usb_claim_interface(devh, 0);
    if (ret != 0) {
	printf("Could not open usb device, errorcode - %d\n", ret);
	exit(1);
    }
    ret = usb_set_altinterface(devh, 0);
    assert(ret >= 0);
}


void _init_wread() {
	char tbuf[1000];
	ret = usb_get_descriptor(devh, 1, 0, tbuf, 0x12);
	// usleep(14*1000);
	ret = usb_get_descriptor(devh, 2, 0, tbuf, 9);
	// usleep(10*1000);
	ret = usb_get_descriptor(devh, 2, 0, tbuf, 0x22);
	// usleep(22*1000);
	ret = usb_release_interface(devh, 0);
	if (ret != 0) printf("failed to release interface before set_configuration: %d\n", ret);
	ret = usb_set_configuration(devh, 1);
	ret = usb_claim_interface(devh, 0);
	if (ret != 0) printf("claim after set_configuration failed with error %d\n", ret);
	ret = usb_set_altinterface(devh, 0);
	// usleep(22*1000);
	ret = usb_control_msg(devh, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0xa, 0, 0, tbuf, 0, 1000);
	// usleep(4*1000);
	ret = usb_get_descriptor(devh, 0x22, 0, tbuf, 0x74);
}

void _send_usb_msg( char msg1[1],char msg2[1],char msg3[1],char msg4[1],char msg5[1],char msg6[1],char msg7[1],char msg8[1] ) {
	char tbuf[1000];
	tbuf[0] = msg1[0];
	tbuf[1] = msg2[0];
	tbuf[2] = msg3[0];
	tbuf[3] = msg4[0];
	tbuf[4] = msg5[0];
	tbuf[5] = msg6[0];
	tbuf[6] = msg7[0];
	tbuf[7] = msg8[0];
	// print_bytes(tbuf, 8);
	// printf(" - - - \n");
	ret = usb_control_msg(devh, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 9, 0x200, 0, tbuf, 8, 1000);
	// usleep(28*1000);
}

void _read_usb_msg(char *buffer) {
   char tbuf[1000];
   usb_interrupt_read(devh, 0x81, tbuf, 0x20, 1000);
   memcpy(buffer, tbuf, 0x20);
   // usleep(82*1000);
}


void read_arguments(int argc, char **argv) {
	int c,pinfo=0;
	char *mempos1=0,*endptr;
  	shownone=0;
  	o1=0;
	while ((c = getopt (argc, argv, "akwosiurthp:zyx")) != -1)
	{
         switch (c)
           {
           case 'a':
  	     showall=1;
	     shownone=1;
             break;
           case 'i':
  	     o1=1;
	     shownone=1;
             break;
           case 'u':
  	     o2=1;
	     shownone=1;
             break;
           case 't':
  	     o3=1;
	     shownone=1;
             break;
           case 'w':
  	     o4=1;
	     shownone=1;
             break;
           case 'r':
  	     o5=1;
	     shownone=1;
             break;
           case 'o':
  	     o6=1;
	     shownone=1;
             break;
           case 's':
  	     o7=1;
	     shownone=1;
             break;
           case 'j':
  	     o9=1;
	     shownone=1;
             break;
           case 'p':
  	     mempos1=optarg;
             break;
           case 'h':
  	     pinfo=1;
             break;
           case 'x':
  	     pdebug=1;
             break;
           case 'y':
             postprocess=1;
             shownone=1;
             break;
           case 'z':
  	     resetws=1;
	     shownone=1;
             break;
           case '?':
             if (isprint (optopt))
               fprintf (stderr, "Unknown option `-%c'.\n", optopt);
             else
               fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
           default:
             abort ();
           }
	}
	if ( (pinfo!=0) | (shownone==0) ) {
		printf("Wireless Weather Station Reader v0.1\n");
		printf("(C) 2007 Michael Pendec\n\n");
		printf("options\n");
		printf(" -h	help information\n");
		printf(" -p	Start at offset (can be used together with below parameters\n");
		printf(" -x	Show bytes retrieved from device\n");
		printf(" -z	Reset log buffer (will ask for confirmation.\n\n");
		printf(" -a	Show all stats (overrides below parameters)\n");
		printf(" -s	Show current history position\n");
		printf(" -t	Show temperature\n");
		printf(" -j	Show Pressure (hPa)\n");
		printf(" -u	Show humidity\n");
		printf(" -r	Show rain\n");
		printf(" -w	Show wind\n");
		printf(" -o	other \n\n");
		exit(0);
	}
	if (mempos1!=0) {
	    	mempos = strtol(mempos1, &endptr, 16);
	} else {
		printf("Reading last updated record from device\n");
	}
}

int main(int argc, char **argv) {
    int		buftemp;
    char ec='n';
    struct tempstat buf5;

    read_arguments(argc,argv);
    _open_readw();
   _init_wread();

   if (resetws==1) {
     printf(" Resetting WetterStation history\n");
     printf("Sure you want to reset wetter station (y/N)?");
     fflush(stdin);
     scanf("%c",&ec);
     if ( (ec=='y') || (ec=='Y') ) {
   	_send_usb_msg("\xa0","\x00","\x00","\x20","\xa0","\x00","\x00","\x20");
    	_send_usb_msg("\x55","\x55","\xaa","\xff","\xff","\xff","\xff","\xff");
	usleep(28*1000);
     	_send_usb_msg("\xff","\xff","\xff","\xff","\xff","\xff","\xff","\xff");
	usleep(28*1000);
     	_send_usb_msg("\x05","\x20","\x01","\x38","\x11","\x00","\x00","\x00");
	usleep(28*1000);
    	_send_usb_msg("\x00","\x00","\xaa","\x00","\x00","\x00","\x20","\x3e");
	usleep(28*1000);
     } else {
     	printf(" Aborted reset of history buffer\n");
     }
     _close_readw();
     return 0;
   }
   _send_usb_msg("\xa1","\x00","\x00","\x20","\xa1","\x00","\x00","\x20");
   _read_usb_msg(buf2);
   if ( ( pdebug==1) ) 
   {
      printf("000-031: ");
      print_bytes(buf2, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\x20","\x20","\xa1","\x00","\x20","\x20");
   _read_usb_msg(buf2+32);
   if ( ( pdebug==1) ) 
   {
      printf("032-063: ");
      print_bytes(buf2+32, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\x40","\x20","\xa1","\x00","\x40","\x20");
   _read_usb_msg(buf2+64);
   if ( ( pdebug==1) ) 
   {
      printf("064-095: ");
      print_bytes(buf2+64, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\x60","\x20","\xa1","\x00","\x60","\x20");
   _read_usb_msg(buf2+96);
   if ( ( pdebug==1) ) 
   {
      printf("096-123: ");
      print_bytes(buf2+96, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\x80","\x20","\xa1","\x00","\x80","\x20");
   _read_usb_msg(buf2+128);
   if ( ( pdebug==1) ) 
   {
      printf("124-159: ");
      print_bytes(buf2+128, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\xa0","\x20","\xa1","\x00","\xa0","\x20");
   _read_usb_msg(buf2+160);
   if ( ( pdebug==1) ) 
   {
      printf("160-191: ");
      print_bytes(buf2+160, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\xc0","\x20","\xa1","\x00","\xc0","\x20");
   _read_usb_msg(buf2+192);
   if ( ( pdebug==1) ) 
   {
      printf("192-223: ");
      print_bytes(buf2+192, 32);
      printf("\n");
   }
   _send_usb_msg("\xa1","\x00","\xe0","\x20","\xa1","\x00","\xe0","\x20");
   _read_usb_msg(buf2+224);
   if ( ( pdebug==1) ) 
   {
      printf("224-255: ");
      print_bytes(buf2+224, 32);
      printf("\n");
   }


 //  buf4.noffset = (unsigned char) buf2[22] + ( 256 * buf2[23] );
   buf4.noffset = (unsigned char) buf2[30] + ( 256 * buf2[31] );
   if (mempos!=0) buf4.noffset = mempos;
   buftemp = 0;
   if (buf4.noffset!=0) buftemp = buf4.noffset - 0x10;
   buf[1] = ( buftemp >>8 & 0xFF ) ;
   buf[2] = buftemp & 0xFF;
   buf[3] = ( buftemp >>8 & 0xFF ) ;
   buf[4] = buftemp & 0xFF;
   _send_usb_msg("\xa1",buf+1,buf+2,"\x20","\xa1",buf+3,buf+4,"\x20");
   _read_usb_msg(buf2+224);
   if ( ( pdebug==1) ) 
   {
      printf("224-255: ");
      print_bytes(buf2+224, 32);
      printf("\n");
   }

ret = usb_control_msg(devh, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0x0000009, 0x0000200, 0x0000000, buf, 0x0000008, 1000);
// usleep(8*1000);
ret = usb_interrupt_read(devh, 0x00000081, buf, 0x0000020, 1000);
memcpy(buf2+256, buf, 0x0000020);
if ( ( pdebug==1) )
{
   printf("256-287: ");
   print_bytes(buf2+256, 32);
   printf("\n");
}

buf4.delay1 = buf2[224];
buf4.tempi=buf2[236];
	if (buf4.tempi==0) strcpy(buf4.winddirection,"N");
	if (buf4.tempi==1) strcpy(buf4.winddirection,"NNE");
	if (buf4.tempi==2) strcpy(buf4.winddirection,"NE");
	if (buf4.tempi==3) strcpy(buf4.winddirection,"ENE");
	if (buf4.tempi==4) strcpy(buf4.winddirection,"E");
	if (buf4.tempi==5) strcpy(buf4.winddirection,"ESE");
	if (buf4.tempi==6) strcpy(buf4.winddirection,"SE");
	if (buf4.tempi==7) strcpy(buf4.winddirection,"SSE");
	if (buf4.tempi==8) strcpy(buf4.winddirection,"S");
	if (buf4.tempi==9) strcpy(buf4.winddirection,"SSW");
	if (buf4.tempi==10) strcpy(buf4.winddirection,"SW");
	if (buf4.tempi==11) strcpy(buf4.winddirection,"WSW");
	if (buf4.tempi==12) strcpy(buf4.winddirection,"W");
	if (buf4.tempi==13) strcpy(buf4.winddirection,"WNW");
	if (buf4.tempi==14) strcpy(buf4.winddirection,"NW");
	if (buf4.tempi==15) strcpy(buf4.winddirection,"NNW");
buf4.hindoor = buf2[225];
buf4.tindoor =( (unsigned char) buf2[226] + (unsigned char) buf2[227] *256);
buf4.houtdoor = buf2[228];
buf4.toutdoor =( (unsigned char) buf2[229] + (unsigned char) buf2[230] *256);
buf4.pressure = (unsigned char) buf2[231] + ( 256*buf2[232]);
buf4.swind = buf2[233];
buf4.swind2 = buf2[234];
buf4.oth1  = buf2[235];
buf4.rain2 = (unsigned char) buf2[237];
buf4.rain =( (unsigned char) buf2[238] + (unsigned char) buf2[239] *256);
buf4.rain1 = buf2[238];
buf4.oth2 = buf2[239];

//------------------
buf5.delay1 = buf2[240];
buf5.tempi=buf2[252];
	if (buf5.tempi==0) strcpy(buf5.winddirection,"N");
	if (buf5.tempi==1) strcpy(buf5.winddirection,"NNE");
	if (buf5.tempi==2) strcpy(buf5.winddirection,"NE");
	if (buf5.tempi==3) strcpy(buf5.winddirection,"ENE");
	if (buf5.tempi==4) strcpy(buf5.winddirection,"E");
	if (buf5.tempi==5) strcpy(buf5.winddirection,"ESE");
	if (buf5.tempi==6) strcpy(buf5.winddirection,"SE");
	if (buf5.tempi==7) strcpy(buf5.winddirection,"SSE");
	if (buf5.tempi==8) strcpy(buf5.winddirection,"S");
	if (buf5.tempi==9) strcpy(buf5.winddirection,"SSW");
	if (buf5.tempi==10) strcpy(buf5.winddirection,"SW");
	if (buf5.tempi==11) strcpy(buf5.winddirection,"WSW");
	if (buf5.tempi==12) strcpy(buf5.winddirection,"W");
	if (buf5.tempi==13) strcpy(buf5.winddirection,"WNW");
	if (buf5.tempi==14) strcpy(buf5.winddirection,"NW");
	if (buf5.tempi==15) strcpy(buf5.winddirection,"NNW");
buf5.hindoor = buf2[241];
buf5.tindoor =( (unsigned char) buf2[242] + (unsigned char) buf2[243] *256);
buf5.tindoor &= 32767;
//buf5.tindoor = (unsigned char) buf2[242];
buf5.houtdoor = buf2[244];
buf5.toutdoor =( (unsigned char) buf2[245] + (unsigned char) buf2[246] *256);
buf5.toutdoor &= 32767;
//buf5.toutdoor = (unsigned char) buf2[245];
buf5.pressure = (unsigned char) buf2[247] + ( 256*buf2[248]);
buf5.swind = buf2[249];
buf5.swind2 = buf2[250];
buf5.oth1  = buf2[251];
buf5.rain2 = (unsigned char) buf2[253];
buf5.rain =( (unsigned char) buf2[254] + (unsigned char) buf2[255] *256);
buf5.rain1 = buf2[254];
buf5.oth2 = buf2[255];
//------------------
/*
buf4.delay1 = buf2[272];
buf4.tempi=buf2[284];
	if (buf4.tempi==0) strcpy(buf4.winddirection,"N");
	if (buf4.tempi==1) strcpy(buf4.winddirection,"NNE");
	if (buf4.tempi==2) strcpy(buf4.winddirection,"NE");
	if (buf4.tempi==3) strcpy(buf4.winddirection,"ENE");
	if (buf4.tempi==4) strcpy(buf4.winddirection,"E");
	if (buf4.tempi==5) strcpy(buf4.winddirection,"SEE");
	if (buf4.tempi==6) strcpy(buf4.winddirection,"SE");
	if (buf4.tempi==7) strcpy(buf4.winddirection,"SSE");
	if (buf4.tempi==8) strcpy(buf4.winddirection,"S");
	if (buf4.tempi==9) strcpy(buf4.winddirection,"SSW");
	if (buf4.tempi==10) strcpy(buf4.winddirection,"SW");
	if (buf4.tempi==11) strcpy(buf4.winddirection,"SWW");
	if (buf4.tempi==12) strcpy(buf4.winddirection,"W");
	if (buf4.tempi==13) strcpy(buf4.winddirection,"NWW");
	if (buf4.tempi==14) strcpy(buf4.winddirection,"NW");
	if (buf4.tempi==15) strcpy(buf4.winddirection,"NNW");
buf4.hindoor = buf2[273];
buf4.tindoor =( (unsigned char) buf2[274] + (unsigned char) buf2[275] *256);
buf4.houtdoor = buf2[276];
buf4.toutdoor =( (unsigned char) buf2[277] + (unsigned char) buf2[278] *256);
buf4.pressure = (unsigned char) buf2[279] + ( 256*buf2[280]);
buf4.swind = buf2[281];
buf4.swind2 = buf2[282];
buf4.oth1  = buf2[283];
buf4.rain2 = (unsigned char) buf2[285];
buf4.rain =( (unsigned char) buf2[286] + (unsigned char) buf2[287] *256);
buf4.rain1 = buf2[286];
buf4.oth2 = buf2[287];
*/



printf("Last saved values:\n");
unsigned int remain;
if ( (showall==1) | ( o1==1) ) printf("224:\t\tinterval:\t\t%5d\n", buf4.delay1);
if ( (showall==1) | ( o2==1) ) printf("225:\t\tindoor humidity\t\t%5d\n", buf4.hindoor);
if ( (showall==1) | ( o2==1) ) printf("228:\t\toutdoor humidity\t%5d\n", buf4.houtdoor);
remain = buf4.tindoor%10;
if ((signed) remain<0) remain = remain * -1;
if ( (showall==1) | ( o3==1) ) printf("226 227:\tindoor temperature\t%5d.%d\n", buf4.tindoor / 10 ,remain);
remain = buf4.toutdoor%10;

if ((signed) remain<0) remain = remain * -1;
if ( (showall==1) | ( o3==1) ) printf("229 230:\toutdoor temperature\t%5d.%d\n", buf4.toutdoor / 10 ,remain);
remain = buf4.swind%10;
if ( (showall==1) | ( o4==1) ) printf("233:\t\twind speed\t\t%5d.%d\n", buf4.swind / 10 , remain);
remain = buf4.swind2%10;
if ( (showall==1) | ( o4==1) ) printf("234:\t\twind gust\t\t%5d.%d\n", buf4.swind2 / 10 , remain);
if ( (showall==1) | ( o4==1) ) printf("236:\t\twind direction\t\t%5s\n", buf4.winddirection);
remain = buf4.rain%10;
if ( (showall==1) | ( o5==1) ) printf("238 239:\train?\t\t\t%5d.%d\n", buf4.rain / 10 , remain);
remain = (buf4.rain2)%10;
if ( (showall==1) | ( o5==1) ) printf("237:\t\train - ??\t\t%5d.%d\n", buf4.rain2 / 10 , remain);
if ( (showall==1) | ( o5==1) ) printf("248:\t\train1\t\t\t%5d\n", buf4.rain1);
if ( (showall==1) | ( o5==1) ) printf("\t\train2\t\t\t%5d\n", buf4.rain2);
if ( (showall==1) | ( o6==1) ) printf("235:\t\tother 1\t\t\t%5d\n", buf4.oth1);
if ( (showall==1) | ( o6==1) ) printf("239:\t\tother 2\t\t\t%5d\n", buf4.oth2);
remain = buf4.pressure%10;
if ( (showall==1) | ( o9==1) ) printf("231 232:\tpressure(hPa)\t\t%5d.%d\n", buf4.pressure / 10 , remain);
if ( (showall==1) | ( o7==1) ) printf("\t\tCurrent history pos:\t%5x\n", buf4.noffset);
printf("\n");

//------------------
printf("Current values:\n");
if ( (showall==1) | ( o1==1) ) printf("240:\t\tSince last save:\t%5dmin\n", buf5.delay1);
if ( (showall==1) | ( o2==1) ) printf("241:\t\tindoor humidity\t\t%5d\n", buf5.hindoor);
if ( (showall==1) | ( o2==1) ) printf("244:\t\toutdoor humidity\t%5d\n", buf5.houtdoor);
remain = buf5.tindoor%10;
if ((signed) remain<0) remain = remain * -1;
if ( (showall==1) | ( o3==1) ) printf("242 243:\tindoor temperature\t%5d.%d\n", buf5.tindoor / 10 ,remain);
remain = buf5.toutdoor%10;
if ((signed) remain<0) remain = remain * -1;
if ( (showall==1) | ( o3==1) ) printf("245 246:\toutdoor temperature\t%5d.%d\n", buf5.toutdoor / 10 ,remain);
remain = buf5.swind%10;
if ( (showall==1) | ( o4==1) ) printf("249:\t\twind speed\t\t%5d.%d\n", buf5.swind / 10 , remain);
remain = buf5.swind2%10;
if ( (showall==1) | ( o4==1) ) printf("250:\t\twind gust\t\t%5d.%d\n", buf5.swind2 / 10 , remain);
if ( (showall==1) | ( o4==1) ) printf("252:\t\twind direction\t\t%5s\n", buf5.winddirection);
remain = buf5.rain%10;
if ( (showall==1) | ( o5==1) ) printf("254 255:\train? this is always zero %5d.%d\n", buf5.rain / 10 , remain);
remain = (buf5.rain2)%10;
if ( (showall==1) | ( o5==1) ) printf("253:\t\train - 24h?\t\t%5d.%d\n", buf5.rain2 / 10 , remain);
if ( (showall==1) | ( o5==1) ) printf("254:\t\train1\t\t\t%5d\n", buf5.rain1);
if ( (showall==1) | ( o5==1) ) printf("\t\train2\t\t\t%5d\n", buf5.rain2);
if ( (showall==1) | ( o6==1) ) printf("251:\t\tother 1\t\t\t%5d\n", buf5.oth1);
if ( (showall==1) | ( o6==1) ) printf("255:\t\tother 2\t\t\t%5d\n", buf5.oth2);
remain = buf5.pressure%10;
if ( (showall==1) | ( o9==1) ) printf("247 248:\tpressure(hPa)\t\t%5d.%d\n", buf5.pressure / 10 , remain);
if ( (showall==1) | ( o7==1) ) printf("\t\tCurrent history pos:\t%5x\n", buf5.noffset);
printf("\n");


//The only difference is the order of words in printf. This changes the description of values in graphs. Values are processed correctly anyway.
if (postprocess==1) {
printf ("For postprocessing\n");
printf ("Interval\t%d min\n", buf5.delay1);
printf ("Indoor humidity\t%d %%\n", buf5.hindoor);
printf ("Outdoor humidity\t%d %%\n", buf5.houtdoor);
if ((buf2[243] & 128) > 0) {
	printf ("Indoor temperature\t-%d.%d C\n", buf5.tindoor / 10, abs(buf5.tindoor % 10));
} else {
	printf ("Indoor temperature\t%d.%d C\n", buf5.tindoor / 10, abs(buf5.tindoor % 10));
};
if ((buf2[246] & 128) > 0) {
	printf ("Outdoor temperature\t-%d.%d C\n", buf5.toutdoor / 10, abs(buf5.toutdoor % 10));
} else {
	printf ("Outdoor temperature\t%d.%d C\n", buf5.toutdoor / 10, abs(buf5.toutdoor % 10));
};
printf ("Wind speed\t%d.%d m/s\n", buf5.swind / 10, abs(buf5.swind %10));
printf ("Wind gust\t%d.%d m/s\n", buf5.swind2 / 10, abs(buf5.swind %10));
printf ("Wind direction\t%d %s\n", buf2[252], buf5.winddirection);
if (buf5.delay1 != 0) {
	printf ("Rain 1h\t%.1f mm/h\n", (double)((buf5.rain2 - buf4.rain2) + (buf5.rain1 - buf4.rain1)*256)*0.3*(60/buf5.delay1) );
} else {
	printf ("Rain 1h\t%.1f mm/h\n", 0.0);
}
printf ("Rain total\t%.1f  mm\n", (double)(buf5.rain2 + buf5.rain1*256) * 0.3);
printf ("Air pressure\t%d.%d hPa\n", buf5.pressure / 10, buf5.pressure % 10);
printf ("\n");
}
//------------------

_close_readw();
return 0;
}