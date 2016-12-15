#include "cmsis_os.h" // CMSIS RTOS header file
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_audio.h"
#include "math.h"
#include "arm_math.h" // header for DSP library

/*----------------------------------------------------------------------------
 *      Thread 1 'Thread_Name': Sample thread
 *---------------------------------------------------------------------------*/
 #define NUM_CHAN	2 // number of audio channels
 #define NUM_POINTS 1024 // number of points per channel
 #define BUF_LEN NUM_CHAN*NUM_POINTS // length of the audio buffer
/* Private variables ---------------------------------------------------------*/
/* buffer used for audio play */
int16_t Audio_Buffer1[BUF_LEN];
int16_t Audio_Buffer2[BUF_LEN];

int16_t PDMbuf[BUF_LEN*2];
int16_t PCMbuf[BUF_LEN];

void Thread (void const *argument);                             // thread function
osThreadId tid_Thread;                                          // thread id
osThreadDef (Thread, osPriorityNormal, 1, 0);                   // thread object
osSemaphoreDef (SEM0); // Declare semaphore
osSemaphoreId (SEM0_id); // Semaphore ID
#define MSGQUEUE_OBJECTS      1                        // number of Message Queue Objects

osMessageQId mid_MsgQueue;                               // message queue id
osMessageQDef (MsgQueue, MSGQUEUE_OBJECTS, int32_t);     // message queue object

// Initialization function
int Init_Thread (void) {
	
	mid_MsgQueue = osMessageCreate (osMessageQ(MsgQueue), NULL);  // create msg queue
  if (!mid_MsgQueue) {
    ; // Message Queue object not created, handle failure
  }
	
  tid_Thread = osThreadCreate (osThread(Thread), NULL);
  if (!tid_Thread) return(-1);
	SEM0_id = osSemaphoreCreate(osSemaphore(SEM0),0);	//Initialize the semaphore with a zero
	return(0);
}

void Thread (void const *argument) {
	static uint8_t rtrn = 0;  // return variable
	static uint8_t rtrn2 = 0;  // return variable for input
	float32_t Fs = 8000.0; // sample frequency
	//float32_t freq = 250.0; // signal frequency in Hz
	float32_t freq = 200.0; // signal frequency in Hz
	int16_t i; // loop counter 
	float32_t tmp; // factor
	float32_t cnt = 0.0f; // time index
	
	tmp = 6.28f*freq/Fs; // only calc this factor once
	
	// initialize the audio output and input
	rtrn = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 0x46, 8000);
	if (rtrn != AUDIO_OK)return;
	
	rtrn2 = BSP_AUDIO_IN_Init(8000, 16, 2);
	if(rtrn2 != AUDIO_OK)return;
	
	// generate data for the audio buffer 1
			
	BSP_AUDIO_IN_Record((uint16_t *)Audio_Buffer1, BUF_LEN); // Left channel
				
	//BSP_AUDIO_OUT_Play((uint16_t *)Audio_Buffer1, BUF_LEN);
	
	/*
	uint8_t controllingVariable = 2;
	while(1)
	{
		if(controllingVariable == 1)
		{
			// generate data for the audio buffer 1
			BSP_AUDIO_IN_Record((uint16_t *)Audio_Buffer1, BUF_LEN); // Left channel		
			osMessagePut (mid_MsgQueue, controllingVariable, osWaitForever); // Send Message
			controllingVariable=2;
			osSemaphoreWait(SEM0_id, osWaitForever);
		}
		else
		{
			// generate data for the audio buffer 2		
			BSP_AUDIO_IN_Record((uint16_t *)Audio_Buffer2, BUF_LEN); // Left channel
			osMessagePut (mid_MsgQueue, controllingVariable, osWaitForever); // Send Message
			controllingVariable = 1;
			osSemaphoreWait(SEM0_id, osWaitForever);
		}
	}
	
	*/
	//osDelay(5000);
	//BSP_AUDIO_OUT_Pause();
	//osDelay(5000);
	//BSP_AUDIO_OUT_Resume();
	// Start the audio player
	
}

/* User Callbacks: user has to implement these functions in his code if they are needed. */
/* This function is called when the requested data has been completely transferred. */

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

// This function is called when half of the requested buffer has been transferred. 

void    BSP_AUDIO_OUT_HalfTransfer_CallBack(void){
}

 //This function is called when an Interrupt due to transfer error on or peripheral
  // error occurs. 
void    BSP_AUDIO_OUT_Error_CallBack(void){
		while(1){
		}
}

void BSP_AUDIO_IN_HalfTransfer_CallBack(void){
	 BSP_AUDIO_IN_PDMToPCM((uint16_t *)&PDMbuf[0], (uint16_t *)PCMbuf);
	 memcpy(Audio_Buffer1,PDMbuf,BUF_LEN*2);
	 osMessagePut (mid_MsgQueue, 1, 0);
}
 
void BSP_AUDIO_IN_TransferComplete_CallBack(void){
	BSP_AUDIO_IN_PDMToPCM((uint16_t *)&PDMbuf[BUF_LEN], (uint16_t *)PCMbuf);
  memcpy(Audio_Buffer2,PDMbuf,BUF_LEN*2);
  osMessagePut (mid_MsgQueue, 2, 0);
}
void BSP_AUDIO_IN_Error_Callback(void)
{  
  /* This function is called when an Interrupt due to transfer error on or peripheral
     error occurs. */
                while(1){
                                }
}
