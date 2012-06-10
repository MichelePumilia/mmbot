/* Arduino JPEGCamera Library
 * Copyright 2010 SparkFun Electronic
 * Written by Ryan Owens
*/

#include <avr/pgmspace.h>
#include "JPEGCamera.h"
#include <Arduino.h>
#include <SoftwareSerial.h>

//Commands for the LinkSprite Serial JPEG Camera
const char GET_SIZE[5] =			{0x56, 0x00, 0x34, 0x01, 0x00};
const char RESET_CAMERA[4] =		{0x56, 0x00, 0x26, 0x00};
const char TAKE_PICTURE[5] =		{0x56, 0x00, 0x36, 0x01, 0x00};
const char STOP_TAKING_PICS[5] =	{0x56, 0x00, 0x36, 0x01, 0x03};
const char READ_DATA[8] =			{0x56, 0x00, 0x32, 0x0C, 0x00, 0x0A, 0x00, 0x00};
const char COMPRESSION_RATIO[] =	{0x56, 0x00, 0x31, 0x05, 0x01, 0x01, 0x12, 0x04}; //append 0x00 to 0xFF
const char IMAGE_SIZE[] =			{0x56, 0x00, 0x31, 0x05, 0x04, 0x01, 0x00, 0x19}; //append 0 for 640x480, 1 for 320x240, 2 for 160x120

//We read data from the camera in chunks, this is the chunk size
const int read_size=32;

//Make sure the camera is plugged into pins 2 and 3 for Rx/Tx
//SoftwareSerial cameraPort(2,3);
SoftwareSerial cameraPort(2,3);

JPEGCamera::JPEGCamera()
{
}

//Initialize the serial connection
void JPEGCamera::begin(void)
{
	//Camera baud rate is 38400
	cameraPort.begin(38400);
}

//Get the size of the image currently stored in the camera
//pre: response is an empty string. size is a pointer to an integer
//post: response string contains the entire camera response to the GET_SIZE command
//		size is set to the size of the image
//return: number of characters in the response string
//usage: length = camera.getSize(response, &img_size);
int JPEGCamera::getSize(char * response, int * size)
{
	int count=0;
	//Send the GET_SIZE command string to the camera
	count = sendCommand(GET_SIZE, response, 5);
	//Read 4 characters from the camera and add them to the response string
	for(int i=0; i<4; i++)
	{
		while(!cameraPort.available());
		response[count+i]=cameraPort.read();
	}
	//Set the number of characters to return
	count+=4;
	//The size is in the last 2 characters of the response.
	//Parse them and convert the characters to an integer
    *size = response[count-2]*256;
    *size += (int)response[count-1] & 0x00FF;	
	//Send the number of characters in the response back to the calling function
	return count;
}

//Reset the camera
//pre: response is an empty string
//post: response contains the cameras response to the reset command
//returns: number of characters in the response
//Usage: camera.reset(response);
int JPEGCamera::reset(char * response)
{
	return sendCommand(RESET_CAMERA, response, 4);
}

//Take a new picture
//pre: response is an empty string
//post: response contains the cameras response to the TAKE_PICTURE command
//returns: number of characters in the response
//Usage: camera.takePicture(response);
int JPEGCamera::takePicture(char * response)
{
	return sendCommand(TAKE_PICTURE, response, 5);
}

//Stop taking pictures
//pre: response is an empty string
//post: response contains the cameras response to the STOP_TAKING_PICS command
//returns: number of characters in the response
//Usage: camera.stopPictures(response);
int JPEGCamera::stopPictures(char * response)
{
	return sendCommand(STOP_TAKING_PICS, response, 5);
}

//Get the read_size bytes picture data from the camera
//pre: response is an empty string, address is the address of data to grab
//post: response contains the cameras response to the readData command, but the response header is parsed out.
//returns: number of characters in the response
//Usage: camera.readData(response, cur_address);
//NOTE: This function must be called repeatedly until all of the data is read
//See Sample program on how to get the entire image.
int JPEGCamera::readData(char * response, int address)
{
	beginReadData(address);
	return endReadData(response);
}

void JPEGCamera::beginReadData(int address)
{
	//Flush out any data currently in the serial buffer
	cameraPort.flush();
	
	//Send the command to get read_size bytes of data from the current address
	for(int i=0; i<8; i++)cameraPort.print(READ_DATA[i]);
	cameraPort.write((uint8_t)(address>>8));
	cameraPort.write((uint8_t)address);
	cameraPort.write((uint8_t)0x00);
	cameraPort.write((uint8_t)0x00);
	cameraPort.write((uint8_t)(read_size>>8));
	cameraPort.write((uint8_t)read_size);
	cameraPort.write((uint8_t)0x00);
	cameraPort.write((uint8_t)0x0A);

	delay(15);
	
	//Print the data to the serial port. Used for debugging.
	/*
	for(int i=0; i<8; i++)Serial.print(READ_DATA[i]);
	Serial.write(address>>8);
	Serial.write(address);
	Serial.write(0x00);
	Serial.write(0x00);
	Serial.write(read_size>>8);
	Serial.write(read_size);
	Serial.write(0x00);
	Serial.write(0x0A);	
	Serial.println();
	*/
}

int JPEGCamera::endReadData(char* response)
{
	int count=0;

	//Read the response header.
	for(int i=0; i<5; i++){
		while(!cameraPort.available());
		cameraPort.read();
		//Serial.print("0x");
		//Serial.println((int)cameraPort.read(),HEX);
	}

	delay(50);

	//Now read the actual data and add it to the response string.
	//Serial.println("Reading data");
	count=0;
	while(count < read_size)
	{
		while(!cameraPort.available());
		uint8_t r = cameraPort.read();
		//Serial.print("0x");
		//Serial.println(int(r),HEX);
		response[count] = r;
		count+=1;
	}

	//Return the number of characters in the response.
	return count;
}

//Send a command to the camera
//pre: cameraPort is a serial connection to the camera set to 3800 baud
//     command is a string with the command to be sent to the camera
//     response is an empty string
//	   length is the number of characters in the command
//post: response contains the camera response to the command
//return: the number of characters in the response string
//usage: None (private function)
int JPEGCamera::sendCommand(const char * command, char * response, int length)
{
	char c=0;
	int count=0;
	//Clear any data currently in the serial buffer
	cameraPort.flush();
	//Send each character in the command string to the camera through the camera serial port
	for(int i=0; i<length; i++){
		cameraPort.print(*command++);
	}
	//Get the response from the camera and add it to the response string.
	for(int i=0; i<length; i++)
	{
		while(!cameraPort.available());
		uint8_t r = cameraPort.read();
		//Serial.print("0x");
		//Serial.println(int(r),HEX);
		*response++=r;	
		count+=1;
	}
	
	//return the number of characters in the response string
	return count;
}

int JPEGCamera::setImageQuality(int quality_0_to_255, char * response)
{
	//Flush out any data currently in the serial buffer
	cameraPort.flush();

	//Send the command to get read_size bytes of data from the current address
	for(int i=0; i<sizeof(COMPRESSION_RATIO); i++)cameraPort.print(COMPRESSION_RATIO[i]);
	cameraPort.write((uint8_t)quality_0_to_255);

	for(int i=0; i<5; i++)
	{
		while(!cameraPort.available());
		*response++=cameraPort.read();	
	}
	return 5;
}

int JPEGCamera::setImageSize(int size_0_to_2, char * response)
{
	//Flush out any data currently in the serial buffer
	cameraPort.flush();

	//Send the command to get read_size bytes of data from the current address
	for(int i=0; i<sizeof(IMAGE_SIZE); i++)cameraPort.print(IMAGE_SIZE[i]);
	switch(size_0_to_2)
	{
	case 0: cameraPort.write((uint8_t)0x00); break;
	case 1: cameraPort.write((uint8_t)0x11); break;
	case 2: cameraPort.write((uint8_t)0x22); break;
	default: break;
	}

	for(int i=0; i<5; i++)
	{
		while(!cameraPort.available());
		*response++=cameraPort.read();	
	}
	return 5;

}
