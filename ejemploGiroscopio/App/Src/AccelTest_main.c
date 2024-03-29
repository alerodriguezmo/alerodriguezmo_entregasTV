/*
 * AccelTest_main.c
 *
 *  Created on: May 22, 2023
 *      Author: alerodriguezmo
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stm32f4xx.h>
//#include <math.h>

#include "GPIOxDriver.h"
#include "BasicTimer.h"
#include "ExtiDriver.h"
#include "USARTxDriver.h"
#include "PwmDriver.h"
#include "SysTickDriver.h"
#include "I2CDriver.h"

/*	Definición de variables	*/
GPIO_Handler_t handlerLedOk	= {0};
GPIO_Handler_t handlerPinTX = {0};
GPIO_Handler_t handlerPinRX	= {0};

BasicTimer_Handler_t handlerStateOKTimer 	= {0};
BasicTimer_Handler_t handerSampling1KHz		= {0};


USART_Handler_t handlerCommTerminal	= {0};

uint8_t flagSampling1KHz = 0;
uint8_t flagTabulate = 0;
uint8_t flagShowTable = 0;
uint16_t counter2KSamples = 0;
uint8_t rxData = 0;
char bufferData[64] = "Accel MPU-6050 testing...";

uint32_t systemTicks = 0;
uint32_t systemTicksStart = 0;
uint32_t systemTicksEnd = 0;

/*	Configración para el I2C	*/
GPIO_Handler_t handlerI2CSDA = {0};
GPIO_Handler_t handlerI2CSCL = {0};

I2C_Handler_t handlerAccelerometer = {0};
uint8_t i2cBuffer = 0;

#define ACCEL_ADDRESS          	 0b1101001
#define ACCEL_XOUT_L             59
#define ACCEL_XOUT_H             60
#define ACCEL_YOUT_L             61
#define ACCEL_YOUT_H             62
#define ACCEL_ZOUT_L             63
#define ACCEL_ZOUT_H             64

#define PWR_MGMT_1               107
#define WHO_AM_I                 117

////Variables globales para no tener que declararlas siempre en cuanto a coordenadas
int16_t coordinates[3];//Array donde se van a actualizar los datos cada mS de las coordenadas

int16_t coordinatex[2000];//Array donde se van a guardar los datos para x
int16_t coordinatey[2000];//Array donde se van a guardar los datos para y
int16_t coordinatez[2000];//Array donde se van a guardar los datos para z

uint8_t coordinatesbool = 0; //V. Auxiliar que es una flag para calcular las coordenadas

BasicTimer_Handler_t handlerMuestreo = {0};

/*	Definicion de prototipos de funciones	*/
void initSystem(void);
void sampleAt1KHz(void);
void showSampleInLCD(void);
void tabulateSamples(void);
/*
 * Función principal del programa
 * */
int main(void){

	// Llamamos la función que inicializa el sistema
	initSystem();

	/*	Imprimir un mensaje de inicio	*/
	writeMsg(&handlerCommTerminal, bufferData);

	/*	Main loop	*/
	while(1){

		// Hacemos un 'eco' con el valor que nos llega por el serial
		if(rxData != '\0'){
			writeChar(&handlerCommTerminal, rxData);

			if(rxData == 'w'){
				sprintf(bufferData, "WHO AM I? (r)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				i2cBuffer = i2c_readSingleRegister(&handlerAccelerometer, WHO_AM_I);
				sprintf(bufferData, "dataRead = 0x%x \n",(unsigned int) i2cBuffer);
				rxData = '\0';
			}
			else if(rxData == 'p'){
				sprintf(bufferData, "PWR_MGMT_1 state (r)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				i2cBuffer = i2c_readSingleRegister(&handlerAccelerometer, PWR_MGMT_1);
				sprintf(bufferData, "Estado = 0x%x \n", (unsigned int) i2cBuffer);
				writeMsg(&handlerCommTerminal, bufferData);
				rxData = '\0';
			}
			else if(rxData == 'r'){
				sprintf(bufferData, "PWR_MGMT_1 reset (w)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				i2c_writeSingleRegister(&handlerAccelerometer, PWR_MGMT_1, 0x00);
				rxData = '\0';
		}
			else if(rxData == 'x'){
				sprintf(bufferData, "Axis X data (r)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				uint8_t AccelX_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_XOUT_L);
				uint8_t AccelX_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_XOUT_H);
				int16_t AccelX = AccelX_high << 8 | AccelX_low;
				sprintf(bufferData, "AccelX = %d \n", (int) AccelX);
				writeMsg(&handlerCommTerminal, bufferData);
				rxData = '\0';
				}
			else if(rxData == 'y'){
				sprintf(bufferData, "Axis Y data (r)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				uint8_t AccelY_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_YOUT_L);
				uint8_t AccelY_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_YOUT_H);
				int16_t AccelY = AccelY_high << 8 | AccelY_low;
				sprintf(bufferData, "AccelY = %d \n", (int) AccelY);
				writeMsg(&handlerCommTerminal, bufferData);
				rxData = '\0';
				}
			else if(rxData == 'z'){
				sprintf(bufferData, "Axis Z data (r)\n");
				writeMsg(&handlerCommTerminal, bufferData);

				uint8_t AccelZ_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_ZOUT_L);
				uint8_t AccelZ_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_ZOUT_H);
				int16_t AccelZ = AccelZ_high << 8 | AccelZ_low;
				sprintf(bufferData, "AccelZ = %d \n", (int) AccelZ);
				writeMsg(&handlerCommTerminal, bufferData);
				rxData = '\0';
				}
			else{
				rxData = '\0';
			}
		}
	}
}

void initSystem(void){
	// Organizamos la configuración desea del pin que controla el LED de la board
	handlerLedOk.pGPIOx								= GPIOA;
	handlerLedOk.GPIO_PinConfig.GPIO_PinNumber		= PIN_5;
	handlerLedOk.GPIO_PinConfig.GPIO_PinMode		= GPIO_MODE_OUT;
	handlerLedOk.GPIO_PinConfig.GPIO_PinOType		= GPIO_OTYPE_PUSHPULL;
	handlerLedOk.GPIO_PinConfig.GPIO_PinSpeed		= GPIO_OSPEED_FAST;
	handlerLedOk.GPIO_PinConfig.GPIO_PinPuPdControl	= GPIO_PUPDR_NOTHING;

	// Cargamos la configuración
	GPIO_Config(&handlerLedOk);

	// Configuramos los pines sobre los que funciona el USART
	handlerPinTX.pGPIOx								= GPIOA;
	handlerPinTX.GPIO_PinConfig.GPIO_PinNumber		= PIN_2;
	handlerPinTX.GPIO_PinConfig.GPIO_PinMode		= GPIO_MODE_ALTFN;
	handlerPinTX.GPIO_PinConfig.GPIO_PinOType		= GPIO_OTYPE_PUSHPULL;
	handlerPinTX.GPIO_PinConfig.GPIO_PinSpeed		= GPIO_OSPEED_FAST;
	handlerPinTX.GPIO_PinConfig.GPIO_PinPuPdControl	= GPIO_PUPDR_NOTHING;
	handlerPinTX.GPIO_PinConfig.GPIO_PinAltFunMode 	= AF7;

	handlerPinRX.pGPIOx								= GPIOA;
	handlerPinRX.GPIO_PinConfig.GPIO_PinNumber		= PIN_3;
	handlerPinRX.GPIO_PinConfig.GPIO_PinMode		= GPIO_MODE_ALTFN;
	handlerPinRX.GPIO_PinConfig.GPIO_PinOType		= GPIO_OTYPE_PUSHPULL;
	handlerPinRX.GPIO_PinConfig.GPIO_PinSpeed		= GPIO_OSPEED_FAST;
	handlerPinRX.GPIO_PinConfig.GPIO_PinPuPdControl	= GPIO_PUPDR_NOTHING;
	handlerPinRX.GPIO_PinConfig.GPIO_PinAltFunMode 	= AF7;

	// Cargamos las configuraciones
	GPIO_Config(&handlerPinTX);
	GPIO_Config(&handlerPinRX);

	// Configuramos la comunicación serial
	handlerCommTerminal.ptrUSARTx						= USART2;
	handlerCommTerminal.USART_Config.USART_datasize		= USART_DATASIZE_8BIT;
	handlerCommTerminal.USART_Config.USART_parity		= USART_PARITY_NONE;
	handlerCommTerminal.USART_Config.USART_stopbits		= USART_STOPBIT_1;
	handlerCommTerminal.USART_Config.USART_baudrate		= USART_BAUDRATE_115200;
	handlerCommTerminal.USART_Config.USART_mode			= USART_MODE_RXTX;
	handlerCommTerminal.USART_Config.USART_enableIntRX	= USART_RX_INTERRUPT_ENABLE;
	handlerCommTerminal.USART_Config.USART_enableIntTX	= USART_TX_INTERRUPT_DISABLE;

	// Cargamos la configuración
	USART_Config(&handlerCommTerminal);

	// Configuramos el Timer del led OK
	handlerStateOKTimer.ptrTIMx								= TIM2;
	handlerStateOKTimer.TIMx_Config.TIMx_mode				= BTIMER_MODE_UP;
	handlerStateOKTimer.TIMx_Config.TIMx_speed				= BTIMER_SPEED_1ms;
	handlerStateOKTimer.TIMx_Config.TIMx_period				= 250; // Lanza una interrupción cada 250 ms
	handlerStateOKTimer.TIMx_Config.TIMx_interruptEnable	= 1;

	// Cargando la configuración del TIM2
	BasicTimer_Config(&handlerStateOKTimer);

	// Configuramos el Timer para el muestreo
	handlerStateOKTimer.ptrTIMx								= TIM5;
	handlerStateOKTimer.TIMx_Config.TIMx_mode				= BTIMER_MODE_UP;
	handlerStateOKTimer.TIMx_Config.TIMx_speed				= BTIMER_SPEED_1ms;
	handlerStateOKTimer.TIMx_Config.TIMx_period				= 1; // Lanza una interrupción cada 1 ms para un muestreo a 1KHz
	handlerStateOKTimer.TIMx_Config.TIMx_interruptEnable	= 1;

	// Cargando la configuración del TIM2
	BasicTimer_Config(&handlerStateOKTimer);

	// Configurando los pines sobre los que funciona el I2C1
	handlerI2CSCL.pGPIOx								= GPIOB;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinNumber			= PIN_8;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinMode			= GPIO_MODE_ALTFN;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinOType			= GPIO_OTYPE_OPENDRAIN;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinSpeed			= GPIO_OSPEED_FAST;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinPuPdControl	= GPIO_PUPDR_NOTHING;
	handlerI2CSCL.GPIO_PinConfig.GPIO_PinAltFunMode 	= AF4;

	handlerI2CSDA.pGPIOx								= GPIOB;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinNumber			= PIN_9;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinMode			= GPIO_MODE_ALTFN;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinOType			= GPIO_OTYPE_OPENDRAIN;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinSpeed			= GPIO_OSPEED_FAST;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinPuPdControl	= GPIO_PUPDR_NOTHING;
	handlerI2CSDA.GPIO_PinConfig.GPIO_PinAltFunMode 	= AF4;

	// Cargamos las configuraciones
	GPIO_Config(&handlerI2CSCL);
	GPIO_Config(&handlerI2CSDA);

	// Configuramos el protocolo I2C y cargamos dicha configuración
	handlerAccelerometer.ptrI2Cx			= I2C1;
	handlerAccelerometer.modeI2C			= I2C_MODE_FM;
	handlerAccelerometer.slaveAddress		= ACCEL_ADDRESS;

	i2c_config(&handlerAccelerometer);
}

// Función que toma muestras a 1KHz
void sampleAt1KHz(void){
	if(flagSampling1KHz){

		// Se muestrea cada uno de los ejes
	    uint8_t AccelX_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_XOUT_L);
	    uint8_t AccelX_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_XOUT_H);
	    coordinates[0] = AccelX_high << 8 | AccelX_low;

	    uint8_t AccelY_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_YOUT_L);
	    uint8_t AccelY_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_YOUT_H);
	    int16_t AccelY = AccelY_high << 8 | AccelY_low;
	    coordinates[1] = AccelY;

	    uint8_t AccelZ_low = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_ZOUT_L);
	    uint8_t AccelZ_high = i2c_readSingleRegister(&handlerAccelerometer, ACCEL_ZOUT_H);
	    int16_t AccelZ = AccelZ_high << 8 | AccelZ_low;
	    coordinates[2] = AccelZ;

	    // Se aumenta el contador que permite almacenar 2000 datos
	    if(counter2KSamples != 2000){
	    	counter2KSamples++;
	    	}
	    else{
	    	__NOP();
	    }
	    }
}

// Función que captura durante 2s los datos de los 3 ejes del acelerómetro y los imprime
// en la interfaz serial los datos, en formato de tres columnas (x ; y ; z) con los datos separados por el símbolo “;”
void tabulateSamples(void){
	if(flagTabulate == 8){
		coordinatex[counter2KSamples] = coordinates[0];
		coordinatey[counter2KSamples] = coordinates[1];
		coordinatez[counter2KSamples] = coordinates[2];

		flagTabulate = 0;
	}else{
		__NOP();
	}
}

/*Callback relacionado con la recepción del USART2
 * Debo leer el puerto para bajar la bandera de la interrupción
 */
void usart2Rx_Callback(void){
	// Leemos el valor del registro DR, donde se almacena el dato que llega
	// Esto además debe bajar la bandera de la interrupción
	rxData = getRxData();
}

// Callback del timer 2 correspondiente al LED Blinky
void BasicTimer2_Callback(void){
	// Callback del blinky
	GPIOxTogglePin(&handlerLedOk);

	// Callback para la tabulación cada 2s
	flagTabulate++;
}

// Callback del timer 5 correspondiente al muestreo a 1KHz
void BasicTimer5_Callback(void){
	// Callback del sampling
	if(flagSampling1KHz){
		flagSampling1KHz = 0;
	}else{
		flagSampling1KHz = 1;
	}

}
