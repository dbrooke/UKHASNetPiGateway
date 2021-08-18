// RFM69.c
//
// Ported to Arduino 2014 James Coxon
//
// Copyright (C) 2014 Phil Crump
//
// Based on RF22 Copyright (C) 2011 Mike McCauley ported to mbed by Karl Zweimueller
// Based on RFM69 LowPowerLabs (https://github.com/LowPowerLab/RFM69/)

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <gertboard.h>

#include "rfm69.h"
#include "rfm69config.h"

    volatile uint8_t    _mode;

    uint8_t             _sleepMode;
    uint8_t             _idleMode;
    uint8_t             _afterTxMode;
    uint8_t          _channel;
    //SPI                 _spi;
    //InterruptIn         _interrupt;
    uint8_t             _deviceType;

    // These volatile members may get changed in the interrupt service routine
    volatile uint8_t    _bufLen;
    uint8_t             _buf[RFM69_MAX_MESSAGE_LEN];

    volatile boolean    _rxBufValid;

    volatile boolean    _txPacketSent;
    volatile uint8_t    _txBufSentIndex;
  
    volatile uint16_t   _rxBad;
    volatile uint16_t   _rxGood;
    volatile uint16_t   _txGood;

    volatile int        _lastRssi;
    volatile int        _floorRssi;
    volatile uint8_t    _threshold_val;

void spiWrite(uint8_t reg, uint8_t val)
{
	unsigned char data[2];
	
    // noInterrupts();    // Disable Interrupts
    // digitalWrite(_slaveSelectPin, LOW);
    
	data[0] = reg | RFM69_SPI_WRITE_MASK;
	data[1] = val;
	wiringPiSPIDataRW(_channel, data, 2);
	
    // SPI.transfer(reg | RFM69_SPI_WRITE_MASK); // Send the address with the write mask on
    // SPI.transfer(val); // New value follows

    // digitalWrite(_slaveSelectPin, HIGH);
    // interrupts();     // Enable Interrupts
}

uint8_t spiRead(uint8_t reg)
{
	unsigned char data[2];
	uint8_t val;
	
    // noInterrupts();    // Disable Interrupts
    // digitalWrite(_slaveSelectPin, LOW);
    
	data[0] = reg & ~RFM69_SPI_WRITE_MASK;
	data[1] = 0;
	wiringPiSPIDataRW(_channel, data, 2);
	val = data[1];

    // SPI.transfer(reg & ~RFM69_SPI_WRITE_MASK); // Send the address with the write mask off
    // uint8_t val = SPI.transfer(0); // The written value is ignored, reg value is read
    
    // digitalWrite(_slaveSelectPin, HIGH);
    // interrupts();     // Enable Interrupts
	
    return val;
}

void spiBurstRead(uint8_t reg, uint8_t* dest, uint8_t len)
{
	unsigned char data[128];
	int i;
	
    // digitalWrite(_slaveSelectPin, LOW);
    
    // SPI.transfer(reg & ~RFM69_SPI_WRITE_MASK); // Send the start address with the write mask off
    // while (len--)
    //     *dest++ = SPI.transfer(0);
	data[0] = reg & ~RFM69_SPI_WRITE_MASK;
	wiringPiSPIDataRW(_channel, data, len+1);

	for (i=0; i<len; i++)
	{
		dest[i] = data[i+1];
	}
	
    // digitalWrite(_slaveSelectPin, HIGH);
}

void setMode(uint8_t newMode)
{
    spiWrite(RFM69_REG_01_OPMODE, (spiRead(RFM69_REG_01_OPMODE) & 0xE3) | newMode);
    while((spiRead(RFM69_REG_27_IRQ_FLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0)
    {
        printf(".");
    }
    _mode = newMode;
    printf ("Mode = %d\n", spiRead(RFM69_REG_01_OPMODE));
}

void rfm69_handleTimeoutInterrupt()
{
    // RX
    if(_mode == RFM69_MODE_RX) {
        printf("Restart Rx\n");
        spiWrite(RFM69_REG_3D_PACKET_CONFIG2, spiRead(RFM69_REG_3D_PACKET_CONFIG2) | RF_PACKET2_RXRESTART);
    }
}

/*
void RFM69::setModeSleep()
{
    setMode(RFM69_MODE_SLEEP);
}
void RFM69::setModeRx()
{
    setMode(RFM69_MODE_RX);
}
void RFM69::setModeTx()
{
    setMode(RFM69_MODE_TX);
}
uint8_t  RFM69::mode()
{
    return _mode;
}
*/

void clearTxBuf()
{
    // noInterrupts();   // Disable Interrupts
    _bufLen = 0;
    _txBufSentIndex = 0;
    _txPacketSent = false;
    // interrupts();     // Enable Interrupts
}

void clearRxBuf()
{
    // noInterrupts();   // Disable Interrupts
    _bufLen = 0;
    _rxBufValid = false;
    // interrupts();     // Enable Interrupts
}
	
boolean rfm69_init(int chan, int reset_pin, int dio0_pin, int dio4_pin)
{
    int i;

    _idleMode = RFM69_MODE_SLEEP; // Default idle state is SLEEP, our lowest power mode
    _mode = RFM69_MODE_RX; // We start up in RX mode
    _rxGood = 0;
    _rxBad = 0;
    _txGood = 0;
    _channel = chan;
    _afterTxMode = RFM69_MODE_RX;

    if (wiringPiSetup() < 0) {
        fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
        return false;
    }
    if ( wiringPiISR (dio0_pin, INT_EDGE_RISING, &rfm69_handleInterrupt) < 0 ) {
        fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
        return false;
    }
    if ( wiringPiISR (dio4_pin, INT_EDGE_RISING, &rfm69_handleTimeoutInterrupt) < 0 ) {
        fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
        return false;
    }
    if (wiringPiSPISetup(_channel, 500000) < 0)
    {
        fprintf(stderr, "Failed to open SPI port.  Try loading spi library with 'gpio load spi'");
        return false;
    }

    // Reset device
    // first drive pin high
    pinMode(reset_pin, OUTPUT);
    digitalWrite(reset_pin, HIGH);
    // pause for 100 microseconds
    usleep(100);
    // release pin
    pullUpDnControl(reset_pin, PUD_OFF);
    pinMode(reset_pin, INPUT);
    // pause for 5 ms
    usleep(5000);

    // Check for device presence
    if (spiRead(RFM69_REG_10_VERSION) != 0x24)
    {
        return false;
    }

    // Set up device
    for (i=0; CONFIG[i][0] != 255; i++)
    {
        spiWrite(CONFIG[i][0], CONFIG[i][1]);
    }

    _threshold_val = spiRead(RFM69_REG_29_RSSI_THRESHOLD);
    setMode(_mode);

    // interrupt on PayloadReady
    spiWrite(RFM69_REG_25_DIO_MAPPING1, RF_DIOMAPPING1_DIO0_01);

    clearTxBuf();
    clearRxBuf();

    return true;
}

boolean rfm69_available()
{
    return _rxBufValid;
}

void rfm69_handleInterrupt()
{
    // RX
    if(_mode == RFM69_MODE_RX) {
        _lastRssi = rssiRead();
        // PAYLOADREADY (incoming packet)
        if (spiRead(RFM69_REG_28_IRQ_FLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)
        {
            _bufLen = spiRead(RFM69_REG_00_FIFO);
            spiBurstRead(RFM69_REG_00_FIFO, _buf, RFM69_FIFO_SIZE); // Read out full fifo
            _rxGood++;
            _rxBufValid = true;
            // spiWrite(RFM69_REG_28_IRQ_FLAGS2, spiRead(RFM69_REG_28_IRQ_FLAGS2) & ~RF_IRQFLAGS2_PAYLOADREADY);
        }
        // read noise floor and set RSSI threshold
        _floorRssi = rssiMeasure();
    }
    // TX
    else if(_mode == RFM69_MODE_TX) {

        // PacketSent
        if(spiRead(RFM69_REG_28_IRQ_FLAGS2) & RF_IRQFLAGS2_PACKETSENT) {
            _txGood++;
            spiWrite(RFM69_REG_25_DIO_MAPPING1, RF_DIOMAPPING1_DIO0_01);
            setMode(_afterTxMode);
            _txPacketSent = true;
        }
    }
}

/*

void RFM69::isr0()
{
    handleInterrupt ();
}


void RFM69::spiBurstWrite(uint8_t reg, const uint8_t* src, uint8_t len)
{
    digitalWrite(_slaveSelectPin, LOW);
    
    SPI.transfer(reg | RFM69_SPI_WRITE_MASK); // Send the start address with the write mask on
    while (len--)
        SPI.transfer(*src++);
        
    digitalWrite(_slaveSelectPin, HIGH);
}

*/

int rssiRead()
{
    return -spiRead(RFM69_REG_24_RSSI_VALUE)/2;
}

int rssiMeasure()
{
    int count = 0;
    uint8_t rssi_val;

    spiWrite(RFM69_REG_29_RSSI_THRESHOLD, 0xff);
    spiWrite(RFM69_REG_23_RSSI_CONFIG, RF_RSSI_START);
    while((spiRead(RFM69_REG_23_RSSI_CONFIG) & RF_RSSI_DONE) == 0)
    {
        printf(",");
        if(count++ > 100) {
            return 0;
        }
    }
    rssi_val = spiRead(RFM69_REG_24_RSSI_VALUE);
    if(rssi_val - 6 < _threshold_val)
    {
        _threshold_val--;
    }
    else
    {
        _threshold_val++;
    }
    printf("RSSI %d dBm, threshold %d dBm\n", -rssi_val/2, -_threshold_val/2);
    spiWrite(RFM69_REG_29_RSSI_THRESHOLD, _threshold_val);
    spiWrite(RFM69_REG_3D_PACKET_CONFIG2, spiRead(RFM69_REG_3D_PACKET_CONFIG2) | RF_PACKET2_RXRESTART);
    return -rssi_val/2;
}

uint8_t rfmM69_recv(uint8_t* buf, uint8_t len)
{
    // if (!available())
    if (_bufLen == 0)
    {
        len = 0;
    }
    else
    {
        // noInterrupts();   // Disable Interrupts

        if (len > _bufLen)
        {
            len = _bufLen;
        }
        memcpy(buf, _buf, len);
    }
    buf[len] = '\0';
    clearRxBuf();

    // interrupts();     // Enable Interrupts

    return len;
}

/*
void RFM69::startTransmit()
{
    //sendNextFragment(); // Actually the first fragment
    setModeTx(); // Start the transmitter, turns off the receiver
    //Serial.println("Tx Mode Enabled");
    delay(10);
    sendTxBuf();
}

boolean RFM69::send(const uint8_t* data, uint8_t len)
{
    clearTxBuf();
//    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (!fillTxBuf(data, len))
            return false;
        startTransmit();
    }
    spiWrite(RFM69_REG_25_DIO_MAPPING1, RF_DIOMAPPING1_DIO0_00);
    return true;
}

boolean RFM69::fillTxBuf(const uint8_t* data, uint8_t len)
{
    if (((uint16_t)_bufLen + len) > RFM69_MAX_MESSAGE_LEN)
        return false;
    noInterrupts();   // Disable Interrupts
    memcpy(_buf + _bufLen, data, len);
    _bufLen += len;
    interrupts();     // Enable Interrupts
    return true;
}

void RFM69::sendTxBuf() {
    if(_bufLen<RFM69_FIFO_SIZE) {
        uint8_t* src = _buf;
        uint8_t len = _bufLen;
        digitalWrite(_slaveSelectPin, LOW);
	    SPI.transfer(RFM69_REG_00_FIFO | RFM69_SPI_WRITE_MASK); // Send the start address with the write mask on
	    SPI.transfer(len);
    	while (len--)
        	SPI.transfer(*src++);
	    digitalWrite(_slaveSelectPin, HIGH);
    }
}

void RFM69::readRxBuf()
{
    spiBurstRead(RFM69_REG_00_FIFO, _buf, RFM69_FIFO_SIZE);
    _bufLen += RFM69_FIFO_SIZE;
}
*/

int RFM69_lastRssi()
{
    return _lastRssi;
}

/* vim:set et sts=4 sw=4: */
