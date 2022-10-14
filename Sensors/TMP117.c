/*
 * TMP117.c
 *
 *  Created on: Oct 14, 2022
 *      Author: mblack
 */

#include "utilities.h"
#include "TMP117.h"

void *TMP_thread(void *tmp_handle);
void TMP_process_requests(TMP_Handle *handle);
static void i2cErrorHandler(I2C_Transaction *transaction);

void Detect_request(TMP_Handle *tmp_handle, bool *detect);
void Detect_process(TMP_Handle *tmp_handle);
bool Detect_internal(TMP_Handle *tmp_handle);
void ReadTemp_request(TMP_Handle *tmp_handle, float *avgTemp, uint8_t count);
void ReadTemp_process(TMP_Handle *tmp_handle);
uint8_t UnlockMemory_internal(TMP_Handle *tmp_handle);
uint8_t LockMemory_internal(TMP_Handle *tmp_handle);
void ReadSN_request(TMP_Handle *tmp_handle, uint32_t *detect);
void ReadSN_process(TMP_Handle *tmp_handle);
uint32_t ReadSN_internal(TMP_Handle *tmp_handle);
void WriteSN_request(TMP_Handle *tmp_handle, uint32_t serialNo);
void WriteSN_process(TMP_Handle *tmp_handle);
void ReadID_request(TMP_Handle *tmp_handle, uint16_t *id);
void ReadID_process(TMP_Handle *tmp_handle);
void ReadCal_request(TMP_Handle *tmp_handle, float *offset);
void ReadCal_process(TMP_Handle *tmp_handle);
float ReadCal_internal(TMP_Handle *tmp_handle);
void WriteCal_request(TMP_Handle *tmp_handle, float offset);
void WriteCal_process(TMP_Handle *tmp_handle);
void TMP_Stop_request(TMP_Handle *tmp_handle);

/*
 * Initializes TMP thread which runs in parallel
 * to perform all operations on the TMP117
 *
 * Input SysConfig I2C Name Reference (i.e. CONFIG_I2C_0)
 * Input TMP Name (i.e. Probe Temp) up to 10 characters
 *
 */
TMP_Handle Open_TMP(I2C_Handle i2c_handle, uint8_t address, const char TMP_Name[10])
{
    TMP_Handle Tmp_handle;
    Tmp_handle.tmp_status = TMP_Busy;
    Tmp_handle.tmp_request = TMP_Initializing;

    strcpy(Tmp_handle.tmp_name,TMP_Name);
    Tmp_handle.i2c_handle = i2c_handle;
    Tmp_handle.address = address;

    Tmp_handle.Detect = Detect_request;
    Tmp_handle.ReadTemp = ReadTemp_request;
    Tmp_handle.ReadSN = ReadSN_request;
    Tmp_handle.WriteSN = WriteSN_request;
    Tmp_handle.ReadID = ReadID_request;
    Tmp_handle.ReadCal = ReadCal_request;
    Tmp_handle.WriteCal = WriteCal_request;
    Tmp_handle.Stop = TMP_Stop_request;

    Tmp_handle.fxn_details.count = 0;
    Tmp_handle.fxn_details.writeSerialNo = 0;
    Tmp_handle.fxn_details.readSerialNo = NULL;
    Tmp_handle.fxn_details.writeOffset = 0;
    Tmp_handle.fxn_details.readOffset = NULL;
    Tmp_handle.fxn_details.detect = NULL;

    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;

    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 2;
    retc                    = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, 2048);
    if (retc != 0){
        /* failed to set attributes */
        while (1){}
    }

    retc = pthread_create(&(Tmp_handle.pth_handle), &attrs, TMP_thread, (void *)&(Tmp_handle));
    if (retc != 0){
        uart_print_string("!Error: ");
        uart_print_string(Tmp_handle.tmp_name);
        uart_print_string(" Thread creation error!\n");
    }

    return Tmp_handle;
}


/*
 * TMP Thread
 */
void *TMP_thread(void *tmp_handle)
{
    TMP_Handle *Tmp_handle = (TMP_Handle*)tmp_handle;

    Semaphore_Params sem_params;
    Semaphore_Params_init(&sem_params);
    sem_params.mode = Semaphore_Mode_BINARY; // Not using events
    Tmp_handle->sem_handle = Semaphore_create(0, &sem_params, NULL);


    Tmp_handle->i2c_trans.writeBuf = Tmp_handle->fxn_details.txBuffer;
    Tmp_handle->i2c_trans.readBuf = Tmp_handle->fxn_details.rxBuffer;

    while(1){
        Tmp_handle->tmp_status = TMP_Ready;
        Semaphore_pend(Tmp_handle->sem_handle, BIOS_WAIT_FOREVER);
        Tmp_handle->tmp_status = TMP_Busy;
        //uart_print_string("Running ");
        //uart_print_string(Tmp_handle->tmp_name);
        //uart_print_string(" Thread\n");
        TMP_process_requests(Tmp_handle);
    }
}

/*
 * Processes any TMP requests and return once complete
 */
void TMP_process_requests(TMP_Handle *handle)
{
    switch(handle->tmp_request) {
        case TMP_Detect:
            Detect_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_ReadTemp:
            ReadTemp_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_ReadSN:
            ReadSN_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_WriteSN:
            WriteSN_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_ReadID:
            ReadID_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_ReadCal:
            ReadCal_process(handle);
            handle->tmp_request = TMP_None;
            break;
        case TMP_WriteCal:
            WriteCal_process(handle);
            handle->tmp_request = TMP_None;
            break;
        default:
            handle->tmp_request = TMP_None;
            break;
    }
}

/*
 *  ======== i2cErrorHandler ========
 */
static void i2cErrorHandler(I2C_Transaction *transaction)
{
    switch (transaction->status)
    {
        case I2C_STATUS_TIMEOUT:
            uart_print_string("I2C transaction timed out!\n");
            break;
        case I2C_STATUS_CLOCK_TIMEOUT:
            uart_print_string("I2C serial clock line timed out!\n");
            break;
        case I2C_STATUS_ADDR_NACK:
            uart_print_string("I2C slave address not acknowledged!\n");
            break;
        case I2C_STATUS_DATA_NACK:
            uart_print_string("I2C data byte not acknowledged!\n");
            break;
        case I2C_STATUS_ARB_LOST:
            uart_print_string("I2C arbitration to another master!\n");
            break;
        case I2C_STATUS_INCOMPLETE:
            uart_print_string("I2C transaction returned before completion!\n");
            break;
        case I2C_STATUS_BUS_BUSY:
            uart_print_string("I2C bus is already in use!\n");
            break;
        case I2C_STATUS_CANCEL:
            uart_print_string("I2C transaction cancelled!\n");
            break;
        case I2C_STATUS_INVALID_TRANS:
            uart_print_string("I2C transaction invalid!\n");
            break;
        case I2C_STATUS_ERROR:
            uart_print_string("I2C generic error!\n");
            break;
        default:
            uart_print_string("I2C undefined error case!\n");
            break;
    }
}


/*
 * Detect TMP117 request
 */
void Detect_request(TMP_Handle *tmp_handle, bool *detect)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_Detect;
        tmp_handle->fxn_details.detect = detect;
        Semaphore_post(tmp_handle->sem_handle);
    }
}


/*
 * Detect TMP117 process
 */
void Detect_process(TMP_Handle *tmp_handle)
{
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 0;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.resultReg;
    if(I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        uart_print_string("Detected TMP sensor with slave\n");
        *(tmp_handle->fxn_details.detect) = true;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        *(tmp_handle->fxn_details.detect) = false;
    }
}

/*
 * Detect TMP117 for internal use
 */
bool Detect_internal(TMP_Handle *tmp_handle)
{
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 0;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.resultReg;

    if(I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        uart_print_string("Detected TMP sensor with slave\n");
        return true;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return false;
    }
}


/*
 * Read temperature request
 */
void ReadTemp_request(TMP_Handle *tmp_handle, float *avgTemp, uint8_t count)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_ReadTemp;
        tmp_handle->fxn_details.count = count;
        tmp_handle->fxn_details.avgTemp = avgTemp;
        Semaphore_post(tmp_handle->sem_handle);
    }
}

/*
 * Read temperature process
 */
void ReadTemp_process(TMP_Handle *tmp_handle)
{
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;\
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.resultReg;

    int16_t temperature;
    float temp;
    float avg_temp;
    int count_request = tmp_handle->fxn_details.count;
    int sample = 0;
    while(tmp_handle->fxn_details.count){
        tmp_handle->fxn_details.count--;
        if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
            /*
             * Extract degrees C from the received data;
             * see TMP sensor datasheet
             */
            temperature = (tmp_handle->fxn_details.rxBuffer[0] << 8) | \
                    (tmp_handle->fxn_details.rxBuffer[1]);
            temp = temperature;
            temp *= 0.0078125;

            uart_print_string("Value: ");
            uart_print_float(temp);
            uart_print_string("\n");
            if(sample == 0){avg_temp = temp;}
            else{
                avg_temp = (avg_temp*sample + temp)/(sample+1);
            }
            *(tmp_handle->fxn_details.avgTemp) = avg_temp;
        }
        else{
            i2cErrorHandler(&tmp_handle->i2c_trans);
            *(tmp_handle->fxn_details.avgTemp) = -296;
            //return 0;
        }
        sample++;
        /* Sleep for 1s */
        sleep(1);
    }
    if(count_request > 1){
        uart_print_string("Average Value: ");
        uart_print_float(avg_temp);
        uart_print_string("\n");
    }
    //return avg_temp;
}

/*
 * Read ID request
 */
void ReadID_request(TMP_Handle *tmp_handle, uint16_t *id)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_ReadID;
        tmp_handle->fxn_details.readID = id;
        Semaphore_post(tmp_handle->sem_handle);
    }
}

/*
 * Read EUI
 */
void ReadID_process(TMP_Handle *tmp_handle)
{
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.EuiReg;

    uint16_t id;
    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        id = ((tmp_handle->fxn_details.rxBuffer[0] & 0x0F) << 8) | \
                (tmp_handle->fxn_details.rxBuffer[1]);
        *(tmp_handle->fxn_details.readID) = id;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return;
    }
    uart_print_uint32((uint32_t)id);
    uart_print_string("\n");
}


/*
 * Read Calibration Request
 */
void ReadCal_request(TMP_Handle *tmp_handle, float *offset)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_ReadCal;
        tmp_handle->fxn_details.readOffset = offset;
        Semaphore_post(tmp_handle->sem_handle);
    }
}


/*
 * Read Calibration Offset
 */
void ReadCal_process(TMP_Handle *tmp_handle)
{
    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return; //error
    }

    int16_t tempOffset = 0;
    float offset = 0;
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.TempOffsetReg;

    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        tempOffset = (tmp_handle->fxn_details.rxBuffer[0] << 8) | \
                (tmp_handle->fxn_details.rxBuffer[1]);
        offset = (float)tempOffset;
        offset /= 128;
        *(tmp_handle->fxn_details.readOffset) = offset;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
    }

    uart_print_string("Stored Offset: ");
    uart_print_float(offset);
    uart_print_string("\n");
}


/*
 * Read Calibration Offset
 */
float ReadCal_internal(TMP_Handle *tmp_handle)
{
    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return -296; //error
    }

    int16_t tempOffset = 0;
    float offset = 0;
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.TempOffsetReg;

    if(I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        tempOffset = (tmp_handle->fxn_details.rxBuffer[0] << 8) | \
                (tmp_handle->fxn_details.rxBuffer[1]);
        offset = (float)tempOffset;
        offset /= 128;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return -296; //error
    }

    uart_print_string("Stored Offset: ");
    uart_print_float(offset);
    uart_print_string("\n");
    return offset;
}

/*
 * Write calibration offset request
 */
void WriteCal_request(TMP_Handle *tmp_handle, float offset)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_WriteCal;
        tmp_handle->fxn_details.writeOffset = offset;
        Semaphore_post(tmp_handle->sem_handle);
    }
}


/*
 * Write Calibration Offset process
 */
void WriteCal_process(TMP_Handle *tmp_handle)
{
    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return; //error
    }
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 0;
    tmp_handle->i2c_trans.writeCount = 3;
    tmp_handle->fxn_details.txBuffer[0] = sensor.TempOffsetReg;

    int16_t tempOffset = (int16_t)(128*(tmp_handle->fxn_details.writeOffset));

    tmp_handle->fxn_details.txBuffer[1] = (uint8_t)(tempOffset >> 8);
    tmp_handle->fxn_details.txBuffer[2] = (uint8_t)(tempOffset & 0xFF);

    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        usleep(10000); //Wait 10ms
        float read_offset = ReadCal_internal(tmp_handle);
        if(read_offset == tmp_handle->fxn_details.writeOffset){
            uart_print_string("...Successfully applied offset: ");
            uart_print_float(read_offset);
            uart_print_string("\n");
        }
        else{
            uart_print_string("!Error: Calibration offset verification failed");
        }
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
    }
}


/*
 * Read serial number request
 */
void ReadSN_request(TMP_Handle *tmp_handle, uint32_t *serialNo)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_ReadSN;
        tmp_handle->fxn_details.readSerialNo = serialNo;
        Semaphore_post(tmp_handle->sem_handle);
    }
}

/*
 * Read Serial Number
 *      Read Mem1 Greatest Byte and next byte
 *      Read Mem2 Lower byte
 */
void ReadSN_process(TMP_Handle *tmp_handle)
{
    uint32_t serialNo = 0;
    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return; //error
    }

    // Read MSB and MSB-1 from Memory 1 Register
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem1Reg;

    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        serialNo |= (tmp_handle->fxn_details.rxBuffer[0] << 24) | \
                (tmp_handle->fxn_details.rxBuffer[1] << 16);
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return;
    }

    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return; //error
    }

    // Read MSB-2 and LSB from Memory 2 Register
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem2Reg;
    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        serialNo |= (tmp_handle->fxn_details.rxBuffer[0] << 8) | \
                (tmp_handle->fxn_details.rxBuffer[1] << 0);
        *(tmp_handle->fxn_details.readSerialNo) = serialNo;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return;
    }

    uart_print_string("Serial Number: SDS7-");
    uart_print_uint32(serialNo);
    uart_print_string("\n");
}


/*
 * Read Serial Number for internal user
 */
uint32_t ReadSN_internal(TMP_Handle *tmp_handle)
{
    uint32_t serialNo = 0;
    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return 0; //error
    }

    // Read MSB and MSB-1 from Memory 1 Register
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem1Reg;

    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        serialNo |= (tmp_handle->fxn_details.rxBuffer[0] << 24) | \
                (tmp_handle->fxn_details.rxBuffer[1] << 16);
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return 0;
    }

    if(LockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot lock EEPROM!\n");
        return 0; //error
    }

    // Read MSB-2 and LSB from Memory 2 Register
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 2;
    tmp_handle->i2c_trans.writeCount = 1;
    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem2Reg;
    if (I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        serialNo |= (tmp_handle->fxn_details.rxBuffer[0] << 8) | \
                (tmp_handle->fxn_details.rxBuffer[1] << 0);
        return serialNo;
    }
    else{
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return 0;
    }

}


/*
 * Write serial number request
 */
void WriteSN_request(TMP_Handle *tmp_handle, uint32_t serialNo)
{
    uint8_t n = 0;
    while(tmp_handle->tmp_status == TMP_Busy && n<10){
        usleep(1000); // Wait 1ms
    } // Wait up to 10ms
    if(tmp_handle->tmp_status == TMP_Ready){
        tmp_handle->tmp_request = TMP_WriteSN;
        tmp_handle->fxn_details.writeSerialNo = serialNo;
        Semaphore_post(tmp_handle->sem_handle);
    }
}


/*
 * Write serial number
 *      Stores number from 1 to 16,777,215
 *      Read as SDS7-########
 *      Refer to latest Calibration Points Table for SDS7 details
 */
void WriteSN_process(TMP_Handle *tmp_handle)
{
    uint32_t serialNo = tmp_handle->fxn_details.writeSerialNo;
    if(UnlockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot unlock EEPROM!\n");
        return; //error
    }
    tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
    tmp_handle->i2c_trans.readCount = 0;
    tmp_handle->i2c_trans.writeCount = 3;

    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem1Reg;
    tmp_handle->fxn_details.txBuffer[1] = (uint8_t)(serialNo>>24);
    tmp_handle->fxn_details.txBuffer[2] = (uint8_t)(serialNo>>16);

    if(!I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return;
    }

    if(UnlockMemory_internal(tmp_handle)){
        uart_print_string("!Error: Cannot unlock EEPROM!\n");
        return; //error
    }

    tmp_handle->i2c_trans.readCount = 0;
    tmp_handle->i2c_trans.writeCount = 3;
    tmp_handle->fxn_details.txBuffer[0] = sensor.Mem2Reg;
    tmp_handle->fxn_details.txBuffer[1] = (uint8_t)(serialNo>>8);
    tmp_handle->fxn_details.txBuffer[2] = (uint8_t)(serialNo>>0);


    if(!I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
        i2cErrorHandler(&tmp_handle->i2c_trans);
        return;
    }

    usleep(50000); //wait 50ms

    uint32_t readSN = ReadSN_internal(tmp_handle);
    if(readSN == serialNo){
        uart_print_string("...Successfully set the Serial Number: ");
        uart_print_uint32(readSN);
        uart_print_string("\n");
    }
    else{
        uart_print_string("!Error: Verification failed\n");
        return;
    }

    return;
}

/*
 * Stop all processes request
 */
void TMP_Stop_request(TMP_Handle *tmp_handle)
{
    uint8_t n = 0;
    if(tmp_handle->tmp_status == TMP_Ready){
        return;
    }
    else if(tmp_handle->tmp_status == TMP_Busy)
    {
        tmp_handle->fxn_details.count = 0;
    }
}


/*
 * Unlocks EEPROM
 * Sets the EEPROM to unlock and checks the status
 *      Returns mask 0x01 if busy, 0x02 if locked, 0x04 if I2C error, 0x08 if timeout error
 *      Returns 0x00 if ready and unlocked (success)
 */
uint8_t UnlockMemory_internal(TMP_Handle *tmp_handle)
{
    bool EEPROM_busy = false;
    bool EEPROM_locked = true; //assume locked
    int time = 0;
    do{
        // Read from Unlock Memory Register
        tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
        tmp_handle->i2c_trans.readCount = 2;
        tmp_handle->i2c_trans.writeCount = 1;
        tmp_handle->fxn_details.txBuffer[0] = sensor.MemUnlockReg;

        if(I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
            EEPROM_busy = tmp_handle->fxn_details.rxBuffer[0]&(1<<6);       //6th bit 1 if busy, 0 if not busy
            EEPROM_locked = !(tmp_handle->fxn_details.rxBuffer[0]&(1<<7));  //7th bit 1 if unlocked, 0 if locked
        }
        else{
            i2cErrorHandler(&tmp_handle->i2c_trans);
            return (EEPROM_busy)|(EEPROM_locked<<1)|(1<<3);
        }
        if(!EEPROM_busy && !EEPROM_locked){break;} //if already set, break
        if(!EEPROM_busy){ //If not busy
            // Write to Unlock Memory Register to unlock
            tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
            tmp_handle->i2c_trans.readCount = 0;
            tmp_handle->i2c_trans.writeCount = 3;
            tmp_handle->fxn_details.txBuffer[0] = sensor.MemUnlockReg;
            tmp_handle->fxn_details.txBuffer[1] = 1<<7;
            tmp_handle->fxn_details.txBuffer[2] = 0;
            if(!I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
                i2cErrorHandler(&tmp_handle->i2c_trans);
                return (EEPROM_busy)|(EEPROM_locked<<1)|(1<<3); //I2C error
            }
        }
        time++;
        if(time > 15){
            uart_print_string("!Error: Timeout during attempt to unlock memory\n");
            return (EEPROM_busy)|(EEPROM_locked<<1)|(1<<4); //timeout
        }
        usleep(10000); //10ms
    }while(EEPROM_busy || EEPROM_locked);

    return (EEPROM_busy)|(EEPROM_locked<<1);
}

/*
 * Locks EEPROM
 * Sets the EEPROM to lock and checks the status
 *      Returns mask 0x01 if busy, 0x02 if unlocked, 0x04 if I2C error, 0x08 if timeout error
 *      Returns 0x00 if ready and locked (success)
 */
uint8_t LockMemory_internal(TMP_Handle *tmp_handle)
{
    bool EEPROM_busy = false;
    bool EEPROM_unlocked = true; //assume unlocked
    int time = 0;

    do{
        // Read from Unlock Memory Register
        tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
        tmp_handle->i2c_trans.readCount = 2;
        tmp_handle->i2c_trans.writeCount = 1;
        tmp_handle->fxn_details.txBuffer[0] = sensor.MemUnlockReg;
        if(I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
            EEPROM_busy = (tmp_handle->fxn_details.rxBuffer[0])&(1<<6);       //6th bit 1 if busy, 0 if not busy
            EEPROM_unlocked = (tmp_handle->fxn_details.rxBuffer[0]&(1<<7)); //7th bit 1 if unlocked, 0 if locked
        }
        else{
            i2cErrorHandler(&tmp_handle->i2c_trans);
            return (EEPROM_busy)|(EEPROM_unlocked<<1)|(1<<3);
        }
        if(!EEPROM_busy && !EEPROM_unlocked){break;} //if already set, break
        if(!EEPROM_busy){ //If not busy
            // Write to Unlock Memory Register to unlock
            tmp_handle->i2c_trans.slaveAddress = tmp_handle->address;
            tmp_handle->i2c_trans.readCount = 0;
            tmp_handle->i2c_trans.writeCount = 3;
            tmp_handle->fxn_details.txBuffer[0] = sensor.MemUnlockReg;
            tmp_handle->fxn_details.txBuffer[1] = 0;
            tmp_handle->fxn_details.txBuffer[2] = 0;
            if(!I2C_transfer(tmp_handle->i2c_handle, &tmp_handle->i2c_trans)){
                i2cErrorHandler(&tmp_handle->i2c_trans);
                return (EEPROM_busy)|(EEPROM_unlocked<<1)|(1<<3);
            }
        }
        time++;
        if(time > 15){
            uart_print_string("!Error: Timeout during attempt lock memory\n");
            return (EEPROM_busy)|(EEPROM_unlocked<<1)|(0x08); //timeout
        }
        usleep(10000); //10ms
    }while(EEPROM_busy || EEPROM_unlocked);
    return (EEPROM_busy)|(EEPROM_unlocked<<1);
}
