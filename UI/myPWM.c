/*
 * myPWM.c
 *
 *  Created on: Oct 13, 2022
 *      Author: mblack
 */
#include "myPWM.h"
#include "utilities.h"

void *PWM_thread(void *myPWM_handle);
void PWM_process_requests(myPWM_Handle *handle);
void Set_request(myPWM_Handle *handle, uint8_t brightness);
void Set_process(myPWM_Handle *handle);
void Set_internal(myPWM_Handle *handle, uint8_t brightness);
void Blink_request(myPWM_Handle *handle, uint8_t count);
void Blink_process(myPWM_Handle *handle);
void Pulse_request(myPWM_Handle *handle, uint8_t count);
void Pulse_process(myPWM_Handle *handle);
void PWM_Stop_request(myPWM_Handle *handle);


/*
 * Initializes PWM thread which runs in parallel
 * to perform different operations such as blinking,
 * fading, etc..
 *
 * Input SysConfig PWM Name Reference (i.e. CONFIG_PWM_0)
 * Input LED Name (i.e. GREEN LED) up to 10 characters
 *
 * Returns NULL on error else return handle
 * Used the pointer to the handle to control the LED
 */
myPWM_Handle Open_myPWM(uint_least8_t PWM, const char LED_Name[10])
{
    myPWM_Handle myPwm_handle;
    myPwm_handle.pwm_status = PWM_Busy;
    myPwm_handle.pwm_request = PWM_Initializing;

    strcpy(myPwm_handle.LED_Name,LED_Name);
    myPwm_handle.pwm_sysconfig = PWM;

    myPwm_handle.Set = Set_request;
    myPwm_handle.Blink = Blink_request;
    myPwm_handle.Pulse = Pulse_request;
    myPwm_handle.Stop = PWM_Stop_request;

    myPwm_handle.fxn_details.brightness = 0;
    myPwm_handle.fxn_details.count = 0;



    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;

    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 1;
    retc                    = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, 2048);
    if (retc != 0){
        /* failed to set attributes */
        while (1){}
    }

    retc = pthread_create(&(myPwm_handle.pth_handle), &attrs, PWM_thread, (void *)&(myPwm_handle));
    if (retc != 0){
        uart_print_string("!Error: Thread creation error!\n");
    }

    return myPwm_handle;
}


/*
 * PWM Thread
 */
void *PWM_thread(void *myPwm_handle)
{
    myPWM_Handle *myPWM_handle = (myPWM_Handle*)myPwm_handle;

    Semaphore_Params sem_params;
    Semaphore_Params_init(&sem_params);
    sem_params.mode = Semaphore_Mode_BINARY; // Not using events
    myPWM_handle->sem_handle = Semaphore_create(0, &sem_params, NULL);

    PWM_init();
    PWM_Params pwmParams;
    PWM_Params_init(&pwmParams);
    pwmParams.idleLevel = PWM_IDLE_LOW;      // Output low when PWM is not running
    pwmParams.periodUnits = PWM_PERIOD_HZ;   // Period is in Hz
    pwmParams.periodValue = 1e6;             // 1MHz
    pwmParams.dutyUnits = PWM_DUTY_FRACTION; // Duty is in fractional percentage
    pwmParams.dutyValue = 0;                 // 0% initial duty cycle

    // Open the PWM instance
    myPWM_handle->pwm_handle = PWM_open(myPWM_handle->pwm_sysconfig, &pwmParams);

    if (myPWM_handle->pwm_handle == NULL) {
        uart_print_string("!Error: PWM_open failed\n");
        while(1);
    }
    while(1){
        myPWM_handle->pwm_status = PWM_Ready;
        Semaphore_pend(myPWM_handle->sem_handle, BIOS_WAIT_FOREVER);
        myPWM_handle->pwm_status = PWM_Busy;
        //uart_print_string("Running ");
        //uart_print_string(myPWM_handle->LED_Name);
        //uart_print_string(" Thread\n");
        PWM_process_requests(myPWM_handle);
    }
}

/*
 * Processes any PWM requests and return once complete
 */
void PWM_process_requests(myPWM_Handle *handle)
{
    switch(handle->pwm_request) {
        case PWM_Set:
            Set_process(handle);
            handle->pwm_request = PWM_None;
            break;
        case PWM_Blink:
            //uart_print_string("Processing Blink...\n");
            Blink_process(handle);
            handle->pwm_request = PWM_None;
            break;
        case PWM_Pulse:
            Pulse_process(handle);
            handle->pwm_request = PWM_None;
            break;
        default:
            handle->pwm_request = PWM_None;
            break;
    }
}

/*
 * Set green LED Level
 * Input 0 to 100 for 0% to 100% duty cycle
 * Updates the PWM duty cycle of the green LED
 */
void Set_request(myPWM_Handle *handle, uint8_t brightness)
{
    uint8_t n = 0;
    while(handle->pwm_status == PWM_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(handle->pwm_status == PWM_Ready){
        handle->pwm_request = PWM_Set;
        handle->fxn_details.brightness = brightness;
        Semaphore_post(handle->sem_handle);
    }
}

/*
 * Set green LED Level
 * Input 0 to 100 for 0% to 100% duty cycle
 * Updates the PWM duty cycle of the green LED
 */
void Set_process(myPWM_Handle *handle)
{
    if(!handle->fxn_details.brightness){ // if percent is zero
        PWM_stop(handle->pwm_handle);
        return;
    }
    if(handle->fxn_details.brightness > 100){
        handle->fxn_details.brightness = 100;
    }
    PWM_start(handle->pwm_handle);
    uint32_t dutyCycle;
    dutyCycle = (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * handle->fxn_details.brightness) / 100);
    PWM_setDuty(handle->pwm_handle, dutyCycle);
}

/*
 * Set green LED Level
 * Input 0 to 100 for 0% to 100% duty cycle
 * Updates the PWM duty cycle of the green LED
 */
void Set_internal(myPWM_Handle *handle, uint8_t brightness)
{
    if(!brightness){ // if percent is zero
        PWM_stop(handle->pwm_handle);
        return;
    }
    if(brightness > 100){
        brightness = 100;
    }
    PWM_start(handle->pwm_handle);
    uint32_t dutyCycle;
    dutyCycle = (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * brightness) / 100);
    PWM_setDuty(handle->pwm_handle, dutyCycle);
}

/*
 * Blink Green LED
 */
void Blink_request(myPWM_Handle *handle, uint8_t count)
{
    uint8_t n = 0;
    while(handle->pwm_status == PWM_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(handle->pwm_status == PWM_Ready){
        handle->pwm_request = PWM_Blink;
        handle->fxn_details.count = count;
        Semaphore_post(handle->sem_handle);
    }
}

/*
 * Blink Green LED - process called inside thread
 */
void Blink_process(myPWM_Handle *handle)
{
    uint8_t t = 0;
    while(handle->fxn_details.count){
        handle->fxn_details.count--;
        Set_internal(handle, 100);
        usleep(500000); // 500ms
        Set_internal(handle, 0);
        usleep(500000); // 500ms
    }
}


/*
 * Pulse Green LED 3 Times
 */
void Pulse_request(myPWM_Handle *handle, uint8_t count)
{
    uint8_t n = 0;
    while(handle->pwm_status == PWM_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(handle->pwm_status == PWM_Ready){
        handle->pwm_request = PWM_Pulse;
        handle->fxn_details.count = count;
        Semaphore_post(handle->sem_handle);
    }
}

/*
 * Pulse Green LED - process called inside thread
 */
void Pulse_process(myPWM_Handle *handle)
{
    int t = 0;
    while(handle->fxn_details.count){
        handle->fxn_details.count--;
        int n = 0;
        for(n;n<=100;n++){
            Set_internal(handle, n);
            usleep(5000); // 5ms
        } // 500ms rise
        for(n = 100;n >= 0; n--){
            Set_internal(handle, n);
            usleep(5000); // 5ms
        } // 500ms fall
    }
}

/*
 * Stop all processes request
 */
void PWM_Stop_request(myPWM_Handle *handle)
{
    uint8_t n = 0;
    if(handle->pwm_status == PWM_Ready){
        return;
    }
    else if(handle->pwm_status == PWM_Busy)
    {
        handle->fxn_details.count = 0;
    }
}
