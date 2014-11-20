#include <stdio.h>
#include <curl/curl.h>
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
		exit(1);
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

void UploadPacket(char *Packet, int Rssi)
{
	CURL *curl;
	CURLcode res;
	char PostFields[200];

	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();
	if (curl)
	{
		/* First set the URL that is about to receive our POST */
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.ukhas.net/api/upload");

		/* Now specify the POST data */
		sprintf(PostFields, "origin=DBPG&data=%s&rssi=%d", Packet, Rssi);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);

		/* Check for errors */
		if(res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();
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
		if (rfm69_available())
		{
			Bytes = rfmM69_recv(Message, sizeof(Message));
			printf ("Data available - %d bytes\n", Bytes);
			printf ("Line = %s\n", Message);

			// UKHASNet upload
			UploadPacket(Message,RFM69_lastRssi());
			
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

