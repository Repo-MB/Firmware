/*
 * utilities.h
 *
 *  Created on: Oct 13, 2022
 *      Author: mblack
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <ti/drivers/UART2.h>
#include <ti/display/Display.h>

static Display_Handle display;

UART2_Handle uart;
UART2_Params uartParams;

void Read_UART(char *cmd);

void uart_print_string(const char *string);
void uart_print_float(float value);
void uart_print_uint32(uint32_t value);

void reverse(char* str, int len);
int intToStr(int x, char str[], int d);
void ftoa(float n, char* res, int afterpoint);
int stoi(char* string);
float stof(char* string);

#endif /* UTILITIES_H_ */
