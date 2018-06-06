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
#include <stdbool.h>
#include <time.h>


#include "rfm69.h"
#include "rfm69config.h"
#include "minIni.h"

char node_id[20];

extern volatile uint8_t _threshold_val;

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

size_t my_dummy_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

void UploadPacket(char *Packet, int Rssi)
{
	CURL *curl;
	CURLcode res;
	char PostFields[200];
	char *postPacket;

	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();
	if (curl)
	{
		/* First set the URL that is about to receive our POST */
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.ukhas.net/api/upload");

		/* use custom write function to discard response which would otherwise be printed on stdout */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &my_dummy_write);

		/* set transaction timeout of 10 seconds */
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

		/* Now specify the POST data */
		postPacket = curl_easy_escape(curl,Packet,0);
		sprintf(PostFields, "origin=%s&data=%s&rssi=%d", node_id, postPacket, Rssi);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		curl_free(postPacket);

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

bool excluded(char *message, char *exclude)
{
	bool rv;
	char *m, *s;

	m = strdup(message);

	if (strlen(exclude) == 0) {
		/* nothing is excluded */
		rv = false;
	}
	else if ((s = strtok(m, "[")) == NULL) {
		/* m has no node ID */
		rv = true;
	}
	else if ((s = strtok(NULL, ",]")) == NULL) {
		/* m has no terminator for node ID */
		rv = true;
	}
	else if (strstr(exclude, s) == NULL) {
		/* no match for node ID */
		rv = false;
	}
	else {
		/* natch for node ID */
		printf("Excluding \"%s\"\n", s);
		rv = true;
	}

	free(m);
	return rv;
}

int main(int argc, char **argv)
{
	char spin[] = "-\\|/", SequenceCount = 'a';
	char Message[65], Data[100], upload_exclude[200], Command[200], Telemetry[100], *Bracket, *Start;
	int Bytes, Sentence_Count, sc = 0;
	double Latitude, Longitude;
	unsigned int Altitude;
	uint8_t opmode, flags1, flags2, old_opmode = 0, old_flags1 = 0, old_flags2 = 0;
	const char *inifile = "gateway.ini"; // FIXME
	float node_lat, node_lon;
	time_t next_beacon;
	bool bmp085, xap;

	/* load configuration */
	ini_gets("node", "id", "", node_id, sizeof(node_id), inifile);
	node_lat = ini_getf("node", "lat", 999, inifile);
	node_lon = ini_getf("node", "lon", 999, inifile);
	bmp085 = ini_getbool("sensors", "bmp085", false, inifile);
	xap = ini_getbool("xap", "enabled", false, inifile);
	ini_gets("upload", "exclude", "", upload_exclude, sizeof(upload_exclude), inifile);
	
	if (strlen(node_id) == 0) {
		puts("Node ID has not been specified");
		exit(1);
	}

	printf("Upload exclude list is \"%s\"\n", upload_exclude);

	if (xap) {
		xapInit();
	}

	if (node_lat < 900 && node_lon < 900) {
		/* put the gateway on the map */
		sprintf(Message,"0aL%f,%f:%s[%s]", node_lat, node_lon, GIT_VER, node_id);
	} else {
		sprintf(Message,"0a:%s[%s]", GIT_VER, node_id);
	}

	UploadPacket(Message,0);
	if (xap) {
		xapSendPacket(Message, 0);
	}

	if (bmp085) {
		next_beacon = time(NULL) + 10;
		printf("Initialising BMP085\n");
		bmp085_Calibration();
	}

	printf("Initialising RFM69\n");

	setupRFM69();
	
	printf("Starting main loop ...\n");

	Sentence_Count = 0;

	while (1)
	{
		if (rfm69_available())
		{
			Bytes = rfmM69_recv(Message, sizeof(Message));
			printf ("%s Data available - %d bytes\n", Now(), Bytes);
			printf ("rx: %s|%d\n", Message,RFM69_lastRssi());

			if (Bytes > 0)
			{
				if (!excluded(Message, upload_exclude)) {
					// UKHASNet upload
					UploadPacket(Message, RFM69_lastRssi());
				}

				if (xap) {
					// xAP broadcast
					xapSendPacket(Message, RFM69_lastRssi());
				}
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
		}
		else
		{
			opmode = spiRead(RFM69_REG_01_OPMODE);
			flags1 = spiRead(RFM69_REG_27_IRQ_FLAGS1);
			flags2 = spiRead(RFM69_REG_28_IRQ_FLAGS2);
			if ((opmode != old_opmode) || (flags1 != old_flags1) || (flags2 != old_flags2))
			{
				printf ("Registers: %02x, %02x, %02x\n", opmode, flags1, flags2);
				old_opmode = opmode;
				old_flags1 = flags1;
				old_flags2 = flags2;
			}
			else
			{
				printf("\r%c",spin[sc++%4]);
				fflush(stdout);
			}
			if (bmp085 && time(NULL) > next_beacon)
			{
				if (++SequenceCount > 'z')
				{
					SequenceCount = 'b';
				}
				sprintf(Message,"0%cT%0.1fP%dR%d,%d[%s]", SequenceCount, (double)bmp085_GetTemperature(bmp085_ReadUT())/10, bmp085_GetPressure(bmp085_ReadUP()), RFM69_lastRssi(), -_threshold_val/2, node_id);
				UploadPacket(Message,0);
				if (xap) {
					xapSendPacket(Message, 0);
				}
				next_beacon = time(NULL) + 300;
			}
			if (xap) {
				xapCheckHbeat();
			}
			usleep(250000);
		}
	}

	return 0;
}

