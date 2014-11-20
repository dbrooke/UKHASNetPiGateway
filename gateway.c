#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>


#include "rfm69.h"
#include "rfm69config.h"

/*
int AnalogRead (int chan)
{
  unsigned char spiData [3] ;
  unsigned char chanBits ;

  chanBits = 0xC0 | ((chan & 7) << 3);

  spiData [0] = chanBits ;
  spiData [1] = 0;
  spiData[2] = 0;

  wiringPiSPIDataRW (0, spiData, 3) ;

  return ((spiData[0] & 1) << 9) | (spiData[1] << 1) | (spiData[2] >> 7);
}
*/

void setupRFM69()
{  
  
	if (!rfm69_init(0))
	{
		printf("RFM69 Failed\n");
	}
	else
	{
		printf("RFM69 Booted\n");
	}
  
  // attachInterrupt(0, interrupt_function, RISING);
  
  // delay(100);
}

char *Checksum(char *Line)
{
	static char Result[3];
	int Value;
	
	Value = 0;
	for (; *Line; Line++)
	{
		Value ^= *Line;
	}
	
	sprintf(Result, "%02X", Value);
	
	return Result;
}
	
char *Now(void)
{
  time_t rawtime;
  struct tm * timeinfo;
  static char buffer[20];

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (buffer,20,"%H:%M:%S", timeinfo);
  
  return buffer;
}
	
int main(int argc, char **argv)
{
	char Message[65], Data[100], Command[200], Telemetry[100], *Bracket, *Start;
	int Bytes, Sentence_Count;
	double Latitude, Longitude;
	unsigned int Altitude;
	printf("**** UKHASNet Pi Gateway by daveake ****\n");
	
	setupRFM69();
	
	Sentence_Count = 0;

	while (1)
	{
		rfm69_handleInterrupt();
		
		if (rfm69_available())
		{
			Bytes = rfmM69_recv(Message, sizeof(Message));
			printf ("Data available - %d bytes\n", Bytes);
			printf ("Line = %s\n", Message);

			// 3wL51.95023,-2.54445T0R0[DA1]
		
			/*
			// Add our ID to the end of the path
			if (Bracket = strrchr(Message, ']'))
			{
				*Bracket = '\0';
				sprintf(Data, "%s,DA0]", Message);
				printf("Data = %s\n", Data);
			}
			*/

			// UKHASNet upload
			sprintf(Command, "wget -O ukhasnet.txt \"http://www.ukhas.net/api/upload\" --post-data \"origin=DBPG&data=%s\"",	Message);
			printf("%s\n", Command);
			system(Command);
			
#if 0
			// Habitat upload
			// 3dL51.95023,-2.54445,155[DA1]
			if (strstr(Message, "DA1"))
			{
				if (Start = strchr(Message, 'L'))
				{
					// DA1,2,19:35:37,51.95023,-2.54445,160*05%0A

					if (sscanf(Start+1, "%lf,%lf,%u[", &Latitude, &Longitude, &Altitude))
					{
						printf("Altitude = %u\n", Altitude);
						sprintf(Telemetry, "%s,%d,%s,%lf,%lf,%u", "DA1", ++Sentence_Count, Now(), Latitude, Longitude, Altitude);
						sprintf(Command, "wget -O habitat.txt \"http://habitat.habhub.org/transition/payload_telemetry\" --post-data \"callsign=DA0&string=\\$\\$%s*%s%s&string_type=ascii&metadata={}\"",	Telemetry, Checksum(Telemetry), "\%0A");
						printf("%s\n", Command);
						system(Command);
					}
				}
			}
#endif
		}
		else
		{
			printf ("Nothing\n");
			sleep(1);
		}
 	}

	return 0;
}

