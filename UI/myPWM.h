/*
 * myPWM.h
 *
 *  Created on: Oct 13, 2022
 *      Author: mblack
 */

#ifndef MYPWM_H_
#define MYPWM_H_

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <ti/sysbios/BIOS.h>

/* Drivers */
#include <ti/drivers/PWM.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/Board.h> //Sleep header
#include <string.h>

typedef enum myPWM_Request {
    PWM_None,
    PWM_Initializing,
    PWM_Set,
    PWM_Blink,
    PWM_Pulse,
    PWM_Stop
} myPWM_Request;

typedef enum myPWM_Status {
    PWM_Ready,
    PWM_Busy
} myPWM_Status;

typedef struct myPWM_Misc {
    uint8_t count;
    uint8_t brightness;
} myPWM_Misc;

typedef struct myPWM_Handle {
    const char          LED_Name[10];
    pthread_t           pth_handle;     // Thread Handle Generated by Open_myPWM
    uint_least8_t       pwm_sysconfig;  // PWM Name in SysConfig i.e. CONFIG_PWM_0
    PWM_Handle          pwm_handle;     // PWM Handle Generated by Open_myPWM
    Semaphore_Handle    sem_handle;     // Semaphore Handle Generated by Open_myPWM
    myPWM_Status        pwm_status;     // Updated with current status by thread
    myPWM_Request       pwm_request;    // Updated with object methods
    void (*Set)(struct myPWM_Handle*,uint8_t);   // Method to set LED brightness level 0-100%
    void (*Blink)(struct myPWM_Handle*,uint8_t); // Method to blink LED n number of times
    void (*Pulse)(struct myPWM_Handle*,uint8_t); // Method to Pulse LED n number of times
    void (*Stop)(struct myPWM_Handle*);          // Method to stop all current processes
    myPWM_Misc          fxn_details;    // Internal register to manage tasks
} myPWM_Handle;

myPWM_Handle Open_myPWM(uint_least8_t PWM, const char LED_Name[10]);

#endif /* MYPWM_H_ */