#include "cmsis_os.h"  // CMSIS RTOS header file
#include "Board_LED.h"
#include "UART_driver.h"
#include "stdint.h"                     // data type definitions
#include "stdio.h"                      // file I/O functions
#include "rl_usb.h"                     // Keil.MDK-Pro::USB:CORE
#include "rl_fs.h"                      // Keil.MDK-Pro::File System:CORE
#include "stm32f4xx_hal.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// WAVE file header format
typedef struct WAVHEADER {
	unsigned char riff[4];						// RIFF string
	uint32_t overall_size;				// overall size of file in bytes
	unsigned char wave[4];						// WAVE string
	unsigned char fmt_chunk_marker[4];		// fmt string with trailing null char
	uint32_t length_of_fmt;					// length of the format data
	uint16_t format_type;					// format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
	uint16_t channels;						// no.of channels
	uint32_t sample_rate;					// sampling rate (blocks per second)
	uint32_t byterate;						// SampleRate * NumChannels * BitsPerSample/8
	uint16_t block_align;					// NumChannels * BitsPerSample/8
	uint16_t bits_per_sample;				// bits per sample, 8- 8bits, 16- 16 bits etc
	unsigned char data_chunk_header [4];		// DATA string or FLLR string
	uint32_t data_size;						// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
} WAVHEADER;

//*************************************************


//*************************************************
// pointer to file type for files on USB device
FILE *f;

int count = 0;		//TO be deleteted later.
//DEFINE PROTOTYPES
int CurrentState();
int processEvent(int event);
void GetAndSendSong();

//DEFINE STATE AND ACTIONS;
int state = 0;

//define states
#define startState 0
#define chooseSongsState 1
#define PlayingSongState 2
#define pausingState 3

//define actions
#define playSongActivity 1
#define pauseActivity 2
#define loadAndSendSongsActivity 3
#define stopPlaying 4

//define events
#define GetSongPressed 1
#define Play_Button_Pressed 2
#define Pause_Button_Pressed 3
#define exitPressed 4


//DEFINE THREADS
void CommThread (void const *argument);                             // thread function
osThreadId tid_CommThread;                                          // thread id
osThreadDef (CommThread, osPriorityNormal, 1, 0);                   // thread object

void ControlThread (void const *argument);                             // thread function
osThreadId tid_ControlThread;                                          // thread id
osThreadDef (ControlThread, osPriorityNormal, 1, 0);                   // thread object

void ProxyThread (void const *argument);                             // thread function
osThreadId tid_proxyThread;                                          // thread id
osThreadDef (ProxyThread, osPriorityNormal, 1, 0);                   // thread object

//DEFINE QUEUES
//Communication to control thread.
#define COMM_TO_CONTROL_QUEUE_OBJECTS      1                        // number of Message Queue Objects
osMessageQId mid_CommToControlQueue;                               // message queue id
osMessageQDef (CommToControlQueue, COMM_TO_CONTROL_QUEUE_OBJECTS, int32_t);     // message queue object

//Communication to control thread.
#define CONT_TO_PROXY_QUEUE_OBJECTS      1                        // number of Message Queue Objects
osMessageQId mid_ContToProxyQueue;                               // message queue id
osMessageQDef (ContToProxyQueue, CONT_TO_PROXY_QUEUE_OBJECTS, int32_t);     // message queue object

#define MSGQUEUE_OBJECTS 1
osMessageQId mid_MsgQueue;                               // message queue id
osMessageQDef (MsgQueue, MSGQUEUE_OBJECTS, int32_t);     // message queue object

//Wave File Properties
 #define NUM_CHAN	2 // number of audio channels
 #define NUM_POINTS 1024 // number of points per channel
 #define BUF_LEN NUM_CHAN*NUM_POINTS // length of the audio buffer

/* buffer used for audio play */
int16_t Audio_Buffer1[BUF_LEN];
int16_t Audio_Buffer2[BUF_LEN];

//Semaphore Definitions
osSemaphoreDef (SEM0); // Declare semaphore
osSemaphoreId (SEM0_id); // Semaphore ID
char r_dataString[50];	//Song to play received from the GUI.


void Init_Thread (void) {
	state = 0;
	
	//Initializing LEDs and Serial Communication.
	LED_Initialize();
	UART_Init();
	
	//INITIALIZE QUEUES.
	//Initialize COmmunication to Control queue.
	mid_CommToControlQueue = osMessageCreate (osMessageQ(CommToControlQueue), NULL);  // create queue
  if (!mid_CommToControlQueue) {
    ; // Message Queue object not created, handle failure
  }
	
		//Initialize Control to proxy queue.
	mid_ContToProxyQueue = osMessageCreate (osMessageQ(ContToProxyQueue), NULL);  // create queue
  if (!mid_ContToProxyQueue) {
    ; // Message Queue object not created, handle failure
  }
	
	mid_MsgQueue = osMessageCreate (osMessageQ(MsgQueue), NULL);  // create msg queue
  if (!mid_MsgQueue) {
    ; // Message Queue object not created, handle failure
  }
	
	//INITIALIZE THREADS
	//Initialize Communication thread ID
	tid_CommThread = osThreadCreate (osThread(CommThread), NULL);
  if (!tid_CommThread) return;
	
	//Initialize Control thread ID.
	tid_ControlThread = osThreadCreate (osThread(ControlThread), NULL);
  if (!tid_ControlThread) return;
	
	//Initialize Proxy thread
	tid_proxyThread = osThreadCreate (osThread(ProxyThread), NULL);
  if (!tid_proxyThread) return;
	
	SEM0_id = osSemaphoreCreate(osSemaphore(SEM0),0);	//Initialize the semaphore with a zero

	//Turns on when initialization completes.
	LED_On(1);
	
}

/*
*Communication thread is responsible for receiveing commands (inputs from the GUI). 
*It is in an infinite while loop. Whith that it keeps running listening to the 
*serial port. Throufh a message queue, it sends the event to control thread.
*/

void CommThread (void const *argument) {

//*************************************************************************
	//RECEIVE SERIAL DATA
	char r_data[7] = {0,0,0,0,0};
		while (1) {
			
			//Blocks when listening.
			UART_receive(r_data, 5); 
			
			if(!strcmp(r_data,"PAUSE")){
				osMessagePut(mid_CommToControlQueue, Pause_Button_Pressed, osWaitForever);
				
			}
			else if(!strcmp(r_data,"PLAYS")){
				UART_receivestring(r_dataString, 50); 
				osMessagePut(mid_CommToControlQueue, Play_Button_Pressed, osWaitForever);
			}		
			else if(!strcmp(r_data,"GETSO")){			
				osMessagePut(mid_CommToControlQueue, GetSongPressed, osWaitForever);
			}
			else if (!strcmp(r_data,"EXITS"))
			{
				osMessagePut(mid_CommToControlQueue, exitPressed, osWaitForever);
			}
		} // end while
}

/*
* The control thread processes the events and determines what action should be taken.
* When an action is determined, it is added to the queue of the proxy thread. The proxy
* thread react depending on what action it receives.
*/
void ControlThread (void const *argument) {
	osEvent evt; // Receive message object
	uint16_t Action;
	while(1){
		evt = osMessageGet (mid_CommToControlQueue, osWaitForever); // for message
		if (evt.status == osEventMessage) { // check for valid message

			int action = processEvent(evt.value.v);
			
			if(action == loadAndSendSongsActivity){	
				osMessagePut (mid_ContToProxyQueue, loadAndSendSongsActivity, osWaitForever);
			}			
			else if (action == playSongActivity)
					osMessagePut(mid_ContToProxyQueue, playSongActivity, osWaitForever);	
			else if (action == pauseActivity)
				osMessagePut(mid_ContToProxyQueue, pauseActivity, osWaitForever);	
			
			else if (action == stopPlaying)
					osMessagePut(mid_ContToProxyQueue, stopPlaying, osWaitForever);	
				//set the pauseRequest flag to True
			  //BSP_AUDIO_OUT_Pause();
				//pauseRequest = !pauseRequest;
			
	} // end while loop
}
}

/*
* The proxy thread can play, pause, resume, or stop the song depending on what action
* it received from the controller thread.
*/
void ProxyThread (void const *argument) {

	usbStatus ustatus; // USB driver status variable
	uint8_t drivenum = 0; // Using U0: drive number
	char *drive_name = "U0:"; // USB drive name
	fsStatus fstatus; // file system status variable
	osEvent evt; // Receive message object
	osEvent evt1;
	uint16_t Action;
	//static FILE *f;
	
	WAVHEADER header;
	size_t rd;
	uint32_t i;
	static uint8_t rtrn = 0;
	uint8_t rdnum = 1; // read buffer number
	
	
	// initialize the audio output
	rtrn = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 0x46, 44100);
	if (rtrn != AUDIO_OK)return;
	
	LED_On(0);
	ustatus = USBH_Initialize (drivenum); // initialize the USB Host
	if (ustatus == usbOK){
		// loop until the device is OK, may be delay from Initialize
		ustatus = USBH_Device_GetStatus (drivenum); // get the status of the USB device
		while(ustatus != usbOK){
			ustatus = USBH_Device_GetStatus (drivenum); // get the status of the USB device
		}
		// initialize the drive
		fstatus = finit (drive_name);
		if (fstatus != fsOK){
			// handle the error, finit didn't work
		} // end if
		// Mount the drive
		fstatus = fmount (drive_name);
		if (fstatus != fsOK){
			// handle the error, fmount didn't work
		} // end if 
		
		fsFileInfo info;
		info.fileID = 0;
		
		while(1){
			evt = osMessageGet (mid_ContToProxyQueue, osWaitForever); // for message
			if (evt.status == osEventMessage) { // check for valid message	
				if(evt.value.v == loadAndSendSongsActivity){		
						while (ffind ("U0:*.*", &info) == fsOK) {     // find whatever is in drive "U0:"
							UART_send(info.name,strlen(info.name)); 
							UART_send("\n",1);
						}	
				}
				else if(evt.value.v == playSongActivity )
				{	
						//opens the selected file on the GUI
						f = fopen (r_dataString,"r");// open a file on the USB device
						int haveOtherData;
						if (f != NULL) {
							haveOtherData = fread((void *)&header, sizeof(header), 1, f);
						
							//fclose (f); // close the file
						} // end if file opened
						
						if (haveOtherData > 0) {
						haveOtherData = fread((void *)&Audio_Buffer1, sizeof(Audio_Buffer1), 1, f);
						}
						
						BSP_AUDIO_OUT_Play((uint16_t *)Audio_Buffer1, BUF_LEN);
						uint8_t controllingVariable = 2;
					
						while( haveOtherData > 0)
						{
							
								evt = osMessageGet (mid_ContToProxyQueue, 0); // for message
								if (evt.status == osEventMessage) {
									if(evt.value.v == pauseActivity)
									{
										BSP_AUDIO_OUT_Pause();
										BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
										
										evt1 = osMessageGet (mid_ContToProxyQueue, osWaitForever); // for message
											if (evt1.status == osEventMessage) {
												if(evt1.value.v == playSongActivity){
													BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);
													BSP_AUDIO_OUT_Resume();
												}								
											}							
									}
									else if(evt.value.v == playSongActivity)
									{
										fclose(f);
										BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
										
										//opens the selected file on the GUI
										f = fopen (r_dataString,"r");// open a file on the USB device
										if (f != NULL) {
											haveOtherData = fread((void *)&header, sizeof(header), 1, f);
										
											//fclose (f); // close the file
										} // end if file opened
										
										if (haveOtherData > 0) {
										haveOtherData = fread((void *)&Audio_Buffer1, sizeof(Audio_Buffer1), 1, f);
										}
										BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);
										BSP_AUDIO_OUT_Play((uint16_t *)Audio_Buffer1, BUF_LEN);
										controllingVariable = 2;						
									}
									else if (evt.value.v == stopPlaying)
									{
										BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
										fclose(f);	//Close the file.
									}								
							}
							if(controllingVariable == 1)
							{
								// generate data for the audio buffer 1
							haveOtherData = fread((void *)&Audio_Buffer1, sizeof(Audio_Buffer1), 1, f);
								
								osMessagePut (mid_MsgQueue, controllingVariable, osWaitForever); // Send Message
								controllingVariable=2;
								osSemaphoreWait(SEM0_id, osWaitForever);
							}
							else
							{
								// generate data for the audio buffer 2		
								haveOtherData = fread((void *)&Audio_Buffer2, sizeof(Audio_Buffer2), 1, f);
								
								osMessagePut (mid_MsgQueue, controllingVariable, osWaitForever); // Send Message
								controllingVariable = 1;
								osSemaphoreWait(SEM0_id, osWaitForever);
							}
						}

					fclose(f);	//Close the file.
					BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);						
					} // end if USBH_Initialize 
					else if (evt.value.v == stopPlaying)
					{
						BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
						fclose(f);	//Close the file.
					}
				}
			} 
		}
	}

/*The process event function is called by the controller thread. This implements
	*the state chart. It returns the action depending on the current stae and it changes the state.
*/
int processEvent(int event)
{
	if(event ==  GetSongPressed && CurrentState() == startState){
		state = chooseSongsState;
		return loadAndSendSongsActivity;
	}
		else if (event == Play_Button_Pressed && CurrentState() == chooseSongsState){
			state = PlayingSongState;
			return playSongActivity;
		}	
		else if(event == Pause_Button_Pressed && CurrentState() == PlayingSongState)
		{
			state = pausingState;
			return pauseActivity;				
		}
		else if(event == Pause_Button_Pressed && CurrentState() == pausingState)
		{
			state = PlayingSongState;
			return playSongActivity;
		}
			
		else if(event == Play_Button_Pressed && CurrentState() == PlayingSongState)
		{
			state = PlayingSongState;
			return playSongActivity;
		}
		else if (event == exitPressed)
		{
			state = startState;
			return stopPlaying;
		}
}

// It returns the current state.
int CurrentState(){
	return state;
}

void    BSP_AUDIO_OUT_TransferComplete_CallBack(void){
	
	//int32_t controllingVariable ;
	osEvent evt; // receive object
  evt = osMessageGet (mid_MsgQueue, 0);           // wait for message
  if (evt.status == osEventMessage)
	{		// check for valid message
		//evt.value.v;
	
		if(evt.value.v == 1)
				BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer1, BUF_LEN);	
		
				
		else		
			 BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer2, BUF_LEN);	
		
	}
	osSemaphoreRelease(SEM0_id);
//	else
	//	BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)Audio_Buffer1, BUF_LEN);	
	
}

/* This function is called when half of the requested buffer has been transferred. */
void    BSP_AUDIO_OUT_HalfTransfer_CallBack(void){
}

/* This function is called when an Interrupt due to transfer error on or peripheral
   error occurs. */
void    BSP_AUDIO_OUT_Error_CallBack(void){
		while(1){
		}
}


