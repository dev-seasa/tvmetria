/*
 * wwsr - Wireless Weather Station Reader
 * 2007 dec 19, Michael Pendec (michael.pendec@gmail.com)
 * Version 0.5
 * 2008 jan 24 Svend Skafte (svend@skafte.net)
 * 2008 sep 28 Adam Pribyl (covex@lowlevel.cz)
 * Modifications for different firmware version(?)
 * 2009 feb 1 Bradley Jarvis (bradley.jarvis@dcsi.net.au)
 *  major code cleanup
 *  update read to access device discretly
 *  added user formatted output
 *  added log function and fixed debug messaging
 * 2009 mar 28 Lukas Zvonar (lukic@mag-net.sk)
 *  added relative pressure formula (need to compile with -lm switch)
 * 2009 apr 16 Petr Zitny (petr@zitny.net)
 *  added XML output (-x)
 *  fixed new line in help
 *  fixed negative temperature
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <usb.h>
#include <time.h>
#include <math.h>

#define PACKAGE "wwsr"
#define DEFAULT_VENDOR    0x1941
#define DEFAULT_PRODUCT   0x8021
#define DEFAULT_FORMAT    (char *)"time:                  %N\nin humidity:           %h %%\nout humidity:          %H %%\nin temperature:        %t C\nout temperature:       %T C\nout dew temperature:   %C C\nwindchill temperature: %c C\nwind speed:            %W m/s\nwind gust:             %G m/s\nwind direction:        %D\npressure:              %P hPa\nrel. pressure:         %p hPa\nrain:                  %R mm\n"

#define WS_CURRENT_ENTRY  30

int ws_open(usb_dev_handle **dev,uint16_t vendor,uint16_t product);
int ws_close(usb_dev_handle *dev);

int ws_read(usb_dev_handle *dev,uint16_t address,uint8_t *data,uint16_t size);
int ws_reset(usb_dev_handle *dev);
int ws_print(char *format,uint8_t *buffer);
int ws_dump(uint16_t address,uint8_t *buffer,uint16_t size,uint8_t width);

int altitude=0;	//default altitude is sea level - change it if you need, or use -A parameter

typedef enum log_event
{
	LOG_DEBUG=1,
	LOG_WARNING=2,
	LOG_ERROR=4,
	LOG_INFO=8
} log_event;
FILE *_log_debug=NULL,*_log_warning=NULL,*_log_error=NULL,*_log_info=NULL;

void logger(log_event event,char *function,char *msg,...);



int main(int argc, char **argv)
{
	usb_dev_handle *dev;
  int rv,c;
  uint16_t vendor,product;
  uint8_t help;
  
  rv=0;
  dev=NULL;
  vendor=DEFAULT_VENDOR;
  product=DEFAULT_PRODUCT;
  help=0;
  
  _log_error=stderr;
  _log_info=stdout;
  
	while (rv==0 && (c=getopt(argc,argv,"hf:v?d:a:A:x"))!=-1)
	{
		switch (c)
		{
			case 'a': // set device id
				sscanf(optarg,"%X:%X",&vendor,&product);
				logger(LOG_DEBUG,"main","USB device set to vendor=%04X product=%04X",vendor,product);
				break;

			case 'A': // set altitude
				sscanf(optarg,"%d",&altitude);
				logger(LOG_DEBUG,"main","altitude set to %d",altitude);
				break;
			
			case 'v': // Verbose messages
				_log_debug=_log_warning=stdout;
				logger(LOG_DEBUG,"main","Verbose messaging turned on");
				break;
				
			case 'x': // XML export
				optarg = "<data>\
\n\t<temp>\n\t\t<indoor>\n\t\t\t<data>%t</data>\n\t\t\t<unit>C</unit>\n\t\t</indoor>\n\t\t<outdoor>\n\t\t\t<data>%T</data>\n\t\t\t<unit>C</unit>\n\t\t</outdoor>\n\t\t<windchill>\n\t\t\t<data>%c</data>\n\t\t\t<unit>C</unit>\n\t\t</windchill>\n\t\t<dewpoint>\n\t\t\t<data>%C</data>\n\t\t\t<unit>C</unit>\n\t\t</dewpoint>\n\t</temp>\
\n\t<wind>\n\t\t<speed>\n\t\t\t<data>%W</data>\n\t\t\t<unit>m/s</unit>\n\t\t</speed>\n\t\t<gust>\n\t\t\t<data>%G</data>\n\t\t\t<unit>m/s</unit>\n\t\t</gust>\n\t\t<direct>\n\t\t\t<data>%d</data>\n\t\t\t<unit>degrees</unit>\n\t\t</direct>\n\t\t<direct_str>\n\t\t\t<data>%D</data>\n\t\t\t<unit>Str</unit>\n\t\t</direct_str>\n\t</wind>\
\n\t<pressure>\n\t\t<abs>\n\t\t\t<data>%P</data>\n\t\t\t<unit>hPa</unit>\n\t\t</abs>\n\t\t<rel>\n\t\t\t<data>%p</data>\n\t\t\t<unit>hPa</unit>\n\t\t</rel>\n\t</pressure>\
\n\t<rain>\n\t\t<total>\n\t\t\t<data>%R</data>\n\t\t\t<unit>mm</unit>\n\t\t</total>\n\t</rain>\
\n\t<humidity>\n\t\t<indoor>\n\t\t\t<data>%h</data>\n\t\t\t<unit>%%</unit>\n\t\t</indoor>\n\t\t<outdoor>\n\t\t\t<data>%H</data>\n\t\t\t<unit>%%</unit>\n\t\t</outdoor>\n\t</humidity>\
\n</data>\n";
			
			case 'f': // Format output
				logger(LOG_DEBUG,"main","Format output using '%s'",optarg);
				
				if (dev==NULL)
				{
					rv=ws_open(&dev,vendor,product);
				}
				
				if (dev)
				{
					uint16_t address;
					uint8_t buffer[0x10];
					
					ws_read(dev,WS_CURRENT_ENTRY,(unsigned char *)&address,sizeof(address));
					ws_read(dev,address,buffer,sizeof(buffer));
					ws_print(optarg,buffer);
				}
				break;
			
			case 'd': // Dump raw data from weather station
			{
				uint16_t a,s,w;
				
				a=0;
				s=0x100;
				w=16;
				
				if (sscanf(optarg,"0x%X:0x%X",&a,&s)<2)
				if (sscanf(optarg,"0x%X:%u",&a,&s)<2)
				if (sscanf(optarg,"%u:0x%X",&a,&s)<2)
				if (sscanf(optarg,"%u:%u",&a,&s)<2)
				if (sscanf(optarg,":0x%X",&s)<1)
					sscanf(optarg,":%u",&s);
				
				logger(LOG_DEBUG,"main","Dump options address=%u size=%u",a,s);
				
				if (dev==NULL)
				{
					rv=ws_open(&dev,vendor,product);
				}
				
				if (dev)
				{
					uint8_t *b;
					
					logger(LOG_DEBUG,"main","Allocating %u bytes for read buffer",s);
					b=malloc(s);
					if (!b) logger(LOG_ERROR,"main","Could not allocate %u bytes for read buffer",s);
					
					if (b)
					{
						logger(LOG_DEBUG,"main","Allocated %u bytes for read buffer",s);
						
						ws_read(dev,a,b,s);
						ws_dump(a,b,s,w);
						
						free(b);
					}
				}
				break;
			}
			
			case '?':
				help=1;
				printf("Wireless Weather Station Reader v0.1\n");
				printf("(C) 2007 Michael Pendec\n\n");
				printf("options\n");
				printf(" -? Display help\n");
				printf(" -a v:p Change the vendor:product address of the usb device from the default\n");
				printf(" -A <alt in m> Change altitude\n");
				printf(" -v Verbose output, enable debug and warning messages\n");
				printf(" -x XML output\n");
				printf(" -f Format output to user defined string\n");
				printf("    %%h - inside humidity\n");
				printf("    %%H - outside humidity\n");
				printf("    %%t - inside temperature\n");
				printf("    %%T - outside temperature\n");
				printf("    %%C - outside dew temperature\n");
				printf("    %%c - outside wind chill temperature\n");
				printf("    %%W - wind speed\n");
				printf("    %%G - wind gust\n");
				printf("    %%D - wind direction - named\n");
				printf("    %%d - wind direction - degrees\n");
				printf("    %%P - pressure\n");
				printf("    %%p - relative pressure\n");
				printf("    %%R - rain\n");
				printf("    %%N - now - date/time string\n");
				printf(" -d [address]:[length] Dump length bytes from address\n");
		}
	}
	
	if (rv==0 && dev==NULL && help==0)
	{
		uint16_t address;
		uint8_t buffer[0x10];
		
		rv=ws_open(&dev,vendor,product);
		
		if (rv==0) rv=ws_read(dev,WS_CURRENT_ENTRY,(unsigned char *)&address,sizeof(address));
		if (rv==0) rv=ws_read(dev,address,buffer,sizeof(buffer));
		if (rv==0) rv=ws_print(DEFAULT_FORMAT,buffer);
	}
	
	if (dev)
	{
		ws_close(dev);
	}
	
	return rv;
}

int ws_open(usb_dev_handle **dev,uint16_t vendor,uint16_t product)
{
	int rv;
	struct usb_bus *bus;
	
	rv=0;
	*dev=NULL;
	
	logger(LOG_DEBUG,"ws_open","Initialise usb");
	usb_init();
	usb_set_debug(0);
	usb_find_busses();
	usb_find_devices();
	
	logger(LOG_DEBUG,"ws_open","Scan for device %04X:%04X",vendor,product);
	for (bus=usb_get_busses(); bus && *dev==NULL; bus=bus->next)
	{
		struct usb_device *device;
		
		for (device=bus->devices; device && *dev==NULL; device=device->next)
		{
			if (device->descriptor.idVendor == vendor
				&& device->descriptor.idProduct == product)
			{
				logger(LOG_DEBUG,"ws_open","Found device %04X:%04X",vendor,product);
				*dev=usb_open(device);
			}
		}
	}
	
	if (rv==0 && *dev)
	{
		char buf[100];
		
		switch (usb_get_driver_np(*dev,0,buf,sizeof(buf)))
		{
			case 0:
				logger(LOG_WARNING,"ws_open","Interface 0 already claimed by driver \"%s\", attempting to detach it", buf);
				rv=usb_detach_kernel_driver_np(*dev,0);
		}
		
		if (rv==0)
		{
			logger(LOG_DEBUG,"ws_open","Claim device");
			rv=usb_claim_interface(*dev,0);
		}
		
		if (rv==0)
		{
			logger(LOG_DEBUG,"ws_open","Set alt interface");
			rv=usb_set_altinterface(*dev,0);
		}
	}
	else
	{
		logger(LOG_ERROR,"ws_open","Device %04X:%04X not found",vendor,product);
		rv=1;
	}
	
	if (rv==0)
	{
		logger(LOG_DEBUG,"ws_open","Device %04X:%04X opened",vendor,product);
	}
	else
	{
		logger(LOG_ERROR,"ws_open","Device %04X:%04X: could not open, code:%d",vendor,product,rv);
	}
	
	return rv;
}

int ws_close(usb_dev_handle *dev)
{
	int rv;

	if (dev)
	{
		rv=usb_release_interface(dev, 0);
		if (rv!=0) logger(LOG_ERROR,"ws_close","Could not release interface, code:%d", rv);
		
		rv=usb_close(dev);
		if (rv!=0) logger(LOG_ERROR,"ws_close","Error closing interface, code:%d", rv);
	}
	
	return rv;
}

int ws_read(usb_dev_handle *dev,uint16_t address,uint8_t *data,uint16_t size)
{
	uint16_t i,c;
	int rv;
	uint8_t s,tmp[0x20];
	
	memset(data,0,size);
	
	logger(LOG_DEBUG,"ws_read","Reading %d bytes from 0x%04X",size,address);
	
	i=0;
	c=sizeof(tmp);
	s=size-i<c?size-i:c;
	
	for (;i<size;i+=s, s=size-i<c?size-i:c)
	{
		uint16_t a;
		char cmd[9];
		
		a=address+i;
		
		logger(LOG_DEBUG,"ws_read","Send read command: Addr=0x%04X Size=%u",a,s);
		sprintf(cmd,"\xA1%c%c%c\xA1%c%c%c",a>>8,a,c,a>>8,a,c);
		rv=usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,cmd,sizeof(cmd)-1,1000);
		logger(LOG_DEBUG,"ws_read","Sent %d of %d bytes",rv,sizeof(cmd)-1); 
		rv=usb_interrupt_read(dev,0x81,tmp,c,1000);
		logger(LOG_DEBUG,"ws_read","Read %d of %d bytes",rv,c); 
		
		memcpy(data+i,tmp,s);
	}
	
	return 0;
}

int ws_reset(usb_dev_handle *dev)
{
	printf("Resetting WetterStation history\n");
	
	usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,"\xA0\x00\x00\x20\xA0\x00\x00\x20",8,1000);
	usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,"\x55\x55\xAA\xFF\xFF\xFF\xFF\xFF",8,1000);
	//_send_usb_msg("\xa0","\x00","\x00","\x20","\xa0","\x00","\x00","\x20");
	//_send_usb_msg("\x55","\x55","\xaa","\xff","\xff","\xff","\xff","\xff");
	usleep(28*1000);
	
	usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",8,1000);
	//_send_usb_msg("\xff","\xff","\xff","\xff","\xff","\xff","\xff","\xff");
	usleep(28*1000);
	
	//_send_usb_msg("\x05","\x20","\x01","\x38","\x11","\x00","\x00","\x00");
	usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,"\x05\x20\x01\x38\x11\x00\x00\x00",8,1000);
	usleep(28*1000);
	
	//_send_usb_msg("\x00","\x00","\xaa","\x00","\x00","\x00","\x20","\x3e");
	usb_control_msg(dev,USB_TYPE_CLASS+USB_RECIP_INTERFACE,9,0x200,0,"\x00\x00\xAA\x00\x00\x00\x20\x3E",8,1000);
	usleep(28*1000);
	
	return 0;
}

int ws_print(char *format,uint8_t *buffer)
{
        char *dir[]=
        {
                "N","NNE","NE","ENE","E","ESE","SE","SSE",
                "S","SSW","SW","WSW","W","WNW","NW","NNW"
        };
        char *dirdeg[]=
        {
                "0","23","45","68","90","113","135","158",
                "180","203","225","248","270","293","315","338"
        };
/*	char *dirslovak[]=		{"S","SSV","SV","VSV","V","VJV","JV","JJV",
                	                 "J","JJZ","JZ","ZJZ","Z","ZSZ","SZ","SSZ"};	
*/
        time_t basictime;
        char datestring[50];  
        float p,temp,m,windspeed,tw,hum,gama;

	for (;*format;format++)
	{
		if (*format=='%')
		{
			switch (*++format)
			{
				case 'h': // inside humidity
					printf("%d",buffer[0x01]);
					break;
				
				case 'H': // outside humidity
					printf("%d",buffer[0x04]);
					break;
				
				case 't': // inside temperature
					if ((buffer[0x03] & 128) > 0) { //fix negative temperature
					    printf("-%0.1f",(float)((buffer[0x02]+(buffer[0x03]<<8))& 32767)/10);
					} else {
					    printf("%0.1f",(float)((buffer[0x02]+(buffer[0x03]<<8))& 32767)/10);
					};	
					break;
				
				case 'T': // outside temperature
					if ((buffer[0x06] & 128) > 0) { //fix negative temperature
					    printf("-%0.1f",(float)((buffer[0x05]+(buffer[0x06]<<8))& 32767)/10);
					} else {
					    printf("%0.1f",(float)((buffer[0x05]+(buffer[0x06]<<8))& 32767)/10);
					};
					break;	

				case 'C': // dew point based on outside temperature (formula from wikipedia)
                                        temp=(float)(buffer[0x05]+(buffer[0x06]<<8))/10; //out temp celsius
					hum=(float)buffer[0x04]/100; 			 //humidity / 100
					if (hum == 0) hum=0.001;			 //in case of 0% humidity
					gama=(17.271*temp)/(237.7+temp) + log (hum);	 //gama=aT/(b+T) + ln (RH/100)
					tw= (237.7 * gama) / (17.271 - gama);		 //Tdew= (b * gama) / (a - gama)
					printf("%0.1f",tw);
					break;

				case 'c': // windchill temperature
                                        temp=(float)(buffer[0x05]+(buffer[0x06]<<8))/10; //out temp celsius
					windspeed=(float)(buffer[0x09])/10 * 3.6;        //in km/h
					if (( windspeed > 4.8 ) && (temp < 10)) //formula from wikipedia only at condition
                                        tw=13.12 + 0.6215 * temp - 11.37*pow(windspeed,0.16) + 0.3965*temp*pow(windspeed,0.16);
					else tw=temp; 				//else nothing..
					printf("%0.1f",tw);
					break;
				
				case 'W': // wind speed
					printf("%0.1f",(float)(buffer[0x09])/10);
					break;
				
				case 'G': // wind gust
					printf("%0.1f",(float)(buffer[0x0A])/10);
					break;
				
				case 'D': // wind direction - named
					printf(dir[buffer[0x0C]<sizeof(dir)/sizeof(dir[0])?buffer[0x0C]:0]);
					break;

				case 'd': // wind direction - degrees
					printf(dirdeg[buffer[0x0C]<sizeof(dir)/sizeof(dir[0])?buffer[0x0C]:0]);
					break;
				
				case 'P': // pressure
					printf("%0.1f",(float)(buffer[0x07]+(buffer[0x08]<<8))/10);
					break;
				
				case 'p': // rel. pressure
                                        p=(float)(buffer[0x07]+(buffer[0x08]<<8))/10; //abs. pressure
					temp=(float)(buffer[0x05]+(buffer[0x06]<<8))/10; //out temp
					m=altitude / (18429.1 + 67.53 * temp + 0.003 * altitude); //power exponent to correction function
					p=p * pow(10,m); //relative pressure * correction
					printf("%0.1f",p);
					break;
				
				case 'R': // rain
					printf("%0.1f",(float)(buffer[0x0D]+(buffer[0x0E]<<8))*0.3);
					break;

				case 'N': // date
				        time(&basictime);
				        strftime(datestring,sizeof(datestring),"%Y-%m-%d %H:%M:%S",
			                 localtime(&basictime));
				        // Print out and leave
				        printf("%s",datestring);
					break;
				case '%': // percents
				        printf("%%");
					break;
			}
		}
		else if (*format=='\\')
		{
			switch (*++format)
			{
				case 'n':
					printf("\n");
					break;
				
				case 'r':
					printf("\r");
					break;
				
				case 't':
					printf("\t");
					break;
				
			}
		}
		else
		{
			printf("%c",*format);
		}
	}
/*	printf("rain?:           %0.1f (this is always zero)\n",(float)(buffer[253])/10);
	printf("rain:            %0.1f (24h?)\n",(float)(buffer[254]+(buffer[255]<<8))/10);
	printf("rain1:           %d\n",buffer[254]);
	printf("rain2:           %d\n",buffer[253]);
	printf("other 1:         %d\n",buffer[251]);
	printf("other 2:         %d\n",buffer[255]);
	printf("\n");*/
	
	return 0;
}

int ws_dump(uint16_t address,uint8_t *data,uint16_t size,uint8_t w)
{
	uint16_t i,j,s;
	char *buf;
	
	s=8+(w*5)+1;
	logger(LOG_DEBUG,"ws_dump","Allocate %u bytes for temporary buffer",s);
	buf=malloc(s);
	if (!buf) logger(LOG_WARNING,"ws_dump","Could not allocate %u bytes for temporary buffer, verbose dump enabled",s);
	
	logger(LOG_INFO,"ws_dump","Dump %u bytes from address 0x%04X",size,address);
	for (i=0;i<size && buf && data;)
	{
		if (buf) sprintf(buf,"0x%04X:",address+i);
		for (j=0;j<w && i<size;i++,j++)
		{
			if (buf)
			{
				sprintf(buf,"%s 0x%02X",buf,data[i]);
			} else
			{
				logger(LOG_INFO,"ws_dump","0x%04X: 0x%02X",address+i,data[i]);
			}
		}
		if (buf) logger(LOG_INFO,"ws_dump",buf);
	}
	
	return 0;
}

void logger(log_event event,char *function,char *msg,...)
{
	va_list args;
	
	va_start(args,msg);
	switch (event)
	{
		case LOG_DEBUG:
			if (_log_debug)
			{
				fprintf(_log_debug,"message: wwsr.%s - ",function);
				vfprintf(_log_debug,msg,args);
				fprintf(_log_debug,"\n");
			}
			break;
		
		case LOG_WARNING:
			if (_log_warning)
			{
				fprintf(_log_warning,"warning: wwsr.%s - ",function);
				vfprintf(_log_warning,msg,args);
				fprintf(_log_warning,"\n");
			}
			break;
		
		case LOG_ERROR:
			if (_log_error)
			{
				fprintf(_log_error,"error: wwsr.%s - ",function);
				vfprintf(_log_error,msg,args);
				fprintf(_log_error,"\n");
			}
			break;
		
		case LOG_INFO:
			if (_log_info)
			{
				fprintf(_log_info,"info: wwsr.%s - ",function);
				vfprintf(_log_info,msg,args);
				fprintf(_log_info,"\n");
			}
			break;
	}
	va_end(args);
}

