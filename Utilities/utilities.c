/*
 * utilities.c
 *
 *  Created on: Oct 13, 2022
 *      Author: mblack
 */

#include "utilities.h"



/*
 * Blocks on UART response until pressing enter
 * Inputs the UART input string into the cmd pointer
 */
void Read_UART(char *cmd)
{
    char input;
    const char echoPrompt[] = "Echoing characters:\r\n";
    size_t bytesRead;
    size_t bytesWritten = 0;
    uint32_t status     = UART2_STATUS_SUCCESS;

    bool wait = true;
    char command[10] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
    int index = 0;
    /* Loop forever echoing */
    while (wait)
    {
        bytesRead = 0;
        while (bytesRead == 0)
        {
            status = UART2_read(uart, &input, 1, &bytesRead);
            if (status != UART2_STATUS_SUCCESS)
            {
                /* UART2_read() failed */
                return;
            }
        }
        status = UART2_write(uart, &input, 1, &bytesWritten);
        if(input == '\r') {
            strcpy(cmd,command);
            wait=false;
            return;}
        else {
            command[index] = input;
            index++;
        }
    }

}

/*
 * Print string to UART
 */
void uart_print_string(const char *string)
{
    uint32_t status = UART2_STATUS_SUCCESS;
    size_t bytesWritten = 0;
    while (*string != '\0')
    {
        status = UART2_write(uart, string, 1, &bytesWritten);
        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_write() failed */
            return;
        }
        string++;
    }
}


// Reverses a string 'str' of length 'len'
void reverse(char* str, int len)
{
    int i = 0, j = len - 1, temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

// Converts a given integer x to string str[].
// d is the number of digits required in the output.
// If d is more than the number of digits in x,
// then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d) {
        str[i] = '0';
        i++;
    }
    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a character to integer
int ctoi(char character)
{
    switch(character)
    {
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        default:
            return 0;
    }
}
// Converts a string to integer
int stoi(char* string)
{
    int length = strlen(string);
    int value = 0;
    bool negative = false;

    int index = 1;
    for(; index<length+1; index++){
        value += ctoi(*string)*pow(10, length-index);
        string++;
    }
    return value;
}

float stof(char* string)
{
    int length = strlen(string);
    float value = 0;
    int decposition = 0;
    int index = 0;
    for(; index<length; index++){
        if(string[index]=='.'){
            break;
        }
        else{
            decposition++;
        }
    }
    for(index = 0; index<length; index++)
    {
        if(index<decposition){
            value += ctoi(string[index])*pow(10,decposition-(index+1));
        }
        else if(index>decposition){
            value += ctoi(string[index])*pow(10,decposition-(index));
        }
    }
    return value;
}

// Converts a floating-point/double number to a string.
void ftoa(float n, char* res, int afterpoint)
{
    uint8_t d = 0;

    // Extract integer part
    int ipart = (int)n;

    if(ipart == 0){
        d = 1;
    }

    // Extract floating part
    float fpart = n - (float)ipart;

    // convert integer part to string
    int i = intToStr(ipart, res, d);

    // check for display option after point
    if (afterpoint != 0) {
        res[i] = '.'; // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter
        // is needed to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);

        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

/*
 * Print float value to UART
 */
void uart_print_float(float value)
{
    uint32_t status = UART2_STATUS_SUCCESS;
    size_t bytesWritten = 0;
    char *string = malloc(15);
    ftoa(value, string, 3);
    while (*string != '\0')
    {
        status = UART2_write(uart, string, 1, &bytesWritten);
        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_write() failed */
            return;
        }
        string++;
    }
}

/*
 * Print uint32 number to UART
 */
void uart_print_uint32(uint32_t value)
{
    int time = 0;
    uint32_t status = UART2_STATUS_SUCCESS;
    size_t bytesWritten = 0;
    char *string = malloc(15);
    ftoa((float)value, string, 0);
    while (*string != '\0')
    {
        status = UART2_write(uart, string, 1, &bytesWritten);
        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_write() failed */
            return;
        }
        string++;
    }
}


