/**
 * **************************************************************************************************
 * @file     : Alejandro Rodríguez Montes - alerodriguezmo@unal.edu.co
 * @author   : AccelTest_Main.c
 * @brief    : TAREA ESPECIAL - TALLER V (2023-01)
 * **************************************************************************************************
 */

/*	-	-	-	Importación de drivers y elementos necesarios	-	-	-	*/

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stm32f4xx.h>

#include "GPIOxDriver.h"
#include "BasicTimer.h"
#include "ExtiDriver.h"
#include "USARTxDriver.h"
#include "SysTickDriver.h"
#include "I2CDriver.h"
#include "PwmDriver.h"
#include "PLLDriver.h"
#include "DriverLCD.h"

/*	-	-	-	Definición de las macros a utilizar	-	-	-	*/

#define HDC_ADDRESS          	 0x40

#define HDC_TEMPERATURE          0x00
#define HDC_HUMIDITY	         0x01

#define HDC_CONFIG	             0x02
#define HDC_DEVID	             0xFF


#define LCD_ADDRESS				 0x24

/*	-	-	-	Definición de handlers	-	-	-	*/

// Handlers de la comunicacion serial
GPIO_Handler_t handlerPinTX 			= {0};
GPIO_Handler_t handlerPinRX 			= {0};
USART_Handler_t handlerCommTerminal 	= {0};

// Handler del led de estado (Blinky)
GPIO_Handler_t handlerLEDBlinky = {0};

// Handlers de los timers
BasicTimer_Handler_t handlerBlinkyTimer = {0};
BasicTimer_Handler_t handlerSampling	= {0};

// Handlers del protocolo I2C
GPIO_Handler_t handlerI2cSDA = {0};
GPIO_Handler_t handlerI2cSCL = {0};
I2C_Handler_t handlerHDC = {0};
I2C_Handler_t handlerLCD = {0};

/*	-	-	-	Definición de variables	-	-	-	*/
uint16_t temperature_read		= 0;
float temperature				= 0;
uint16_t humidity_read		 	= 0;
float humidity	 			= 0;
char bufferTemp[64] = {0};
char bufferHum[64] = {0};

uint8_t counterLCD = 0;
uint8_t counterSampling = 0;

uint8_t i2cBuffer 		= {0};

// Variables de la comunicación serial por comandos
uint8_t rxData 					= 0;
uint8_t counterReception		= 0;
bool stringComplete 			= false;
int firstParameter				= 0;
int secondParameter				= 0;
int thirdParameter				= 0;
char bufferReception[64]		= {0};
char cmd[64]					= {0};
char bufferData[64]				= {0};

/*	-	-	-	Definición de las cabeceras de las funciones	-	-	-	*/
void tuneMCU(void);
void initSystem(void);
void parseCommands(char *ptrBufferReception);
/*	=	=	=	INICIO DEL MAIN	=	=	=	*/
int main (void){

	// Se afina el MCU
	tuneMCU();

	// Se inicializan todos los sistemas, menos la pantalla LCD

	initSystem();

	// Se inicializa la pantala LCD
	clearScreenLCD(&handlerLCD);

	init_LCD(&handlerLCD);
	delay_10(); // DELAYS CON EL SYSTICK IMPLEMENTADOS EN EL DRIVER DE LA LCD
	clearLCD(&handlerLCD);
	delay_10();

	// Esctirura de los caracteres permanentes

	// Para el eje X
	moveCursor_inLCD(&handlerLCD, 0, 1);
	sendSTR_toLCD(&handlerLCD, "Temp = ");
	moveCursor_inLCD(&handlerLCD, 0, 14);
	sendSTR_toLCD(&handlerLCD, "C");

	// Para el eje Y
	moveCursor_inLCD(&handlerLCD, 1, 1);
	sendSTR_toLCD(&handlerLCD, "Humidity = ");
	moveCursor_inLCD(&handlerLCD, 1, 17);
	sendSTR_toLCD(&handlerLCD, "%");

	delay_ms(20);

	// Se imprime un mensaje de inicio por la terminal serial
	writeMsg(&handlerCommTerminal, "TAREA ESPECIAL - Taller V (2023-01) \n"
			"Alejandro Rodriguez Montes \n"
			"Presione la tecla 'h' para ver los comandos disponibles \n");



	while(1){


		/*	-	-	-	Comunicación por comandos	-	-	-	*/
		if (rxData != '\0'){
			bufferReception[counterReception] = rxData;
			counterReception++;

			// Si el caracter que llega representa un cambio de línea, se levanta una
			// bandera para el loop main
			if (rxData == '@'){
				stringComplete = true;

				// Se agrega el caracter nulo al final del string
				bufferReception[counterReception] = '\0';

				counterReception = 0;
			}

			// Para que no vuelva a entrar. Solo cambia debido a la interrupción
			rxData = '\0';
		}

		// Hacemos un análisis de la cadena de datos obtenida
		else if (stringComplete){
			parseCommands(bufferReception);
			stringComplete = false;
		}

		// Ciclo que muestrea temperatura y humedad
		if(counterSampling > 10){
			temperature_read = i2c_readSingleRegister_16bits(&handlerHDC, HDC_TEMPERATURE);
			humidity_read	 = i2c_readSingleRegister_16bits(&handlerHDC, HDC_HUMIDITY);

			temperature = ((temperature_read / 65536)*165) - 40;
			humidity	= (humidity_read / 65536)*100;

			counterSampling = 0;
		}
		// Ciclo que permite actualizar las lecturas en pantalla
		if(counterLCD > 4){
			sprintf(bufferTemp,"%.2f",temperature);
			sprintf(bufferHum,"%.2f", humidity);

			moveCursor_inLCD(&handlerLCD, 0, 15);
			sendSTR_toLCD(&handlerLCD, bufferTemp);
			moveCursor_inLCD(&handlerLCD, 1, 12);
			sendSTR_toLCD(&handlerLCD, bufferHum);

			counterLCD = 0;
		}

	}
	return(0);
}
/*	=	=	=	FIN DEL MAIN	=	=	=	*/

/*	=	=	=	INICIO DE LA DEFINICIÓN DE FUNCIONES	=	=	=	*/

/*	-	-	-	Función que afina el MCU y establece parámetros de operación	-	-	-	*/
void tuneMCU(void){
	// Se afina el micro para que el HSI quede lo más cercano posible a 16MHz.
	// Por tanteo se llegó a que un trim down de 5 daba los mejores resultados.
	RCC->CR &= ~(RCC_CR_HSITRIM);
	RCC->CR |= 13 << RCC_CR_HSITRIM_Pos;

	// Se activa el coprocesador matematico FPU
	SCB->CPACR |= (0xF << 20);

	// Se configura el systick a 16MHz
	config_SysTick_ms(0);
}


/*	-	-	-	Función que inicializa los elementos del sistema	-	-	-	*/
void initSystem(void){

	/*	-	-	-	Led de estado (Blinky)	-	-	-	*/

	// Configuracion del LED2
	handlerLEDBlinky.pGPIOx                             = GPIOA;
	handlerLEDBlinky.GPIO_PinConfig.GPIO_PinNumber      = PIN_5;
	handlerLEDBlinky.GPIO_PinConfig.GPIO_PinMode        = GPIO_MODE_OUT;
	handlerLEDBlinky.GPIO_PinConfig.GPIO_PinOPType      = GPIO_OTYPE_PUSHPULL;
	handlerLEDBlinky.GPIO_PinConfig.GPIO_PinSpeed       = GPIO_OSPEED_FAST;
	handlerLEDBlinky.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_PUPDR_NOTHING;

	// Se carga la configuración y se enciende por defecto
	GPIO_Config(&handlerLEDBlinky);
	GPIO_WritePin(&handlerLEDBlinky, SET);

	/*	-	-	-	Timers	-	-	-	*/

	// Configuracion del TIM2 para que haga un blinky cada 250ms
	handlerBlinkyTimer.ptrTIMx                               = TIM2;
	handlerBlinkyTimer.TIMx_Config.TIMx_mode                 = BTIMER_MODE_UP;
	handlerBlinkyTimer.TIMx_Config.TIMx_speed                = BTIMER_SPEED_100us;
	handlerBlinkyTimer.TIMx_Config.TIMx_period               = 2500;
	handlerBlinkyTimer.TIMx_Config.TIMx_interruptEnable      = 1;

	// Se carga la configuración y se inicializa el timer
	BasicTimer_Config(&handlerBlinkyTimer);
	StartTimer(&handlerBlinkyTimer);

	// Configuracion del TIM4 para establecer la frecuencia de muestreo en 10Hz
	handlerSampling.ptrTIMx                               = TIM4;
	handlerSampling.TIMx_Config.TIMx_mode                 = BTIMER_MODE_UP;
	handlerSampling.TIMx_Config.TIMx_speed                = BTIMER_SPEED_1ms;
	handlerSampling.TIMx_Config.TIMx_period               = 100;
	handlerSampling.TIMx_Config.TIMx_interruptEnable      = 1;

	// Se carga la configuración y se inicializa el timer
	BasicTimer_Config(&handlerSampling);
	StartTimer(&handlerSampling);

	/*	-	-	-	Protocolo I2C	-	-	-	*/

	// Pin Clock (SCL)
	handlerI2cSCL.pGPIOx                                      = GPIOB;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinNumber               = PIN_8;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinMode                 = GPIO_MODE_ALTFN;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinOPType               = GPIO_OTYPE_OPENDRAIN;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinSpeed                = GPIO_OSPEED_FAST;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinPuPdControl          = GPIO_PUPDR_PULLUP;
	handlerI2cSCL.GPIO_PinConfig.GPIO_PinAltFunMode           = AF4;
	GPIO_Config(&handlerI2cSCL);

	// Pin Data (SDA)
	handlerI2cSDA.pGPIOx                                      = GPIOB;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinNumber               = PIN_9;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinMode                 = GPIO_MODE_ALTFN;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinOPType               = GPIO_OTYPE_OPENDRAIN;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinSpeed                = GPIO_OSPEED_FAST;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinPuPdControl          = GPIO_PUPDR_PULLUP;
	handlerI2cSDA.GPIO_PinConfig.GPIO_PinAltFunMode           = AF4;
	GPIO_Config(&handlerI2cSDA);

	// Configuración del HDC
	handlerHDC.ptrI2Cx                            = I2C1;
	handlerHDC.modeI2C                            = I2C_MODE_FM;
	handlerHDC.slaveAddress                       = HDC_ADDRESS;
	handlerHDC.mainClock						  = MAIN_CLOCK_16_MHz_FOR_I2C;
	handlerHDC.maxI2C_FM						  = I2C_MAX_RISE_TIME_FM_16MHz;
	handlerHDC.modeI2C_FM						  = I2C_MODE_FM_SPEED_400KHz_16MHz;

	i2c_config(&handlerHDC);

	// Configuración del display LCD
	handlerLCD.ptrI2Cx                            = I2C1;
	handlerLCD.modeI2C                            = I2C_MODE_FM;
	handlerLCD.slaveAddress                       = LCD_ADDRESS	;
	handlerLCD.mainClock						  = MAIN_CLOCK_16_MHz_FOR_I2C;
	handlerLCD.maxI2C_FM						  = I2C_MAX_RISE_TIME_FM_16MHz;
	handlerLCD.modeI2C_FM						  = I2C_MODE_FM_SPEED_400KHz_16MHz;

	i2c_config(&handlerLCD);

	/*	-	-	-	Comunicación serial	-	-	-	*/
	handlerPinTX.pGPIOx                               = GPIOA;
	handlerPinTX.GPIO_PinConfig.GPIO_PinNumber        = PIN_2;
	handlerPinTX.GPIO_PinConfig.GPIO_PinMode          = GPIO_MODE_ALTFN;
	handlerPinTX.GPIO_PinConfig.GPIO_PinAltFunMode    = AF7;

	// Se carga la configuración
	GPIO_Config(&handlerPinTX);

	handlerPinRX.pGPIOx                               = GPIOA;
	handlerPinRX.GPIO_PinConfig.GPIO_PinNumber        = PIN_3;
	handlerPinRX.GPIO_PinConfig.GPIO_PinMode          = GPIO_MODE_ALTFN;
	handlerPinRX.GPIO_PinConfig.GPIO_PinAltFunMode    = AF7;

	// Se carga la configuración
	GPIO_Config(&handlerPinRX);

	handlerCommTerminal.ptrUSARTx                       = USART2;
	handlerCommTerminal.USART_Config.USART_baudrate     = USART_BAUDRATE_115200;
	handlerCommTerminal.USART_Config.USART_datasize     = USART_DATASIZE_8BIT;
	handlerCommTerminal.USART_Config.USART_parity       = USART_PARITY_NONE;
	handlerCommTerminal.USART_Config.USART_stopbits     = USART_STOPBIT_1;
	handlerCommTerminal.USART_Config.USART_mode         = USART_MODE_RXTX;
	handlerCommTerminal.USART_Config.USART_enableIntRX  = USART_RX_INTERRUP_ENABLE;
	handlerCommTerminal.USART_Config.USART_enableIntTX  = USART_TX_INTERRUP_DISABLE;
	handlerCommTerminal.USART_Config.USART_frequency    = 16;

	// Se carga la configuración
	USART_Config(&handlerCommTerminal);

}
void parseCommands(char *ptrBufferReception){

	/* Esta función lee la cadena de caracteres a la que apunta el puntero
	 * y almacena en tres elementos diferentes: Un string llamado "cmd", y dos
	 * integers llamados "firstParameter y secondParameter.
	 *
	 * De esta forma, podemos introducir información al micro desde el puerto serial*/
	sscanf(ptrBufferReception, "%s %u %u %u", cmd, &firstParameter, &secondParameter, &thirdParameter);

	// 1) help. Imprime una lista con todos los comandos disponibles
	if(strcmp(cmd, "help") == 0){
		writeMsg(&handlerCommTerminal, "\n");
		writeMsg(&handlerCommTerminal, "HELP MENU - LIST OF AVAILABLE COMMANDS:\n");
		writeMsg(&handlerCommTerminal, "\n");
		writeMsg(&handlerCommTerminal, "1) help\n");
		writeMsg(&handlerCommTerminal, "Print this menu\n");
		writeMsg(&handlerCommTerminal, "\n");
		writeMsg(&handlerCommTerminal, "2) measureTOF\n");
		writeMsg(&handlerCommTerminal, "Measures time of flight of sound to a given obstacle\n");
		writeMsg(&handlerCommTerminal, "\n");
	}
	else{
		// Se imprime el mensaje "Wrong CMD" si la escritura no corresponde a los CMD implementados
		writeMsg(&handlerCommTerminal, "\n");
		writeMsg(&handlerCommTerminal, "Wrong CMD\n");
		writeMsg(&handlerCommTerminal, "\n");
	}
}

/*	=	=	=	FIN DE LA DEFINICIÓN DE FUNCIONES	=	=	=	*/

/*	=	=	=	INICIO DE LAS RUTINAS DE ATENCIÓN (Callbacks)	=	=	=	*/

/* Callbacks de las interrupciones */
void usart6Rx_Callback(void){
	rxData = getRxData();
}

/* Callbacks de los Timers */
void BasicTimer2_Callback(void){
	GPIOxTooglePin(&handlerLEDBlinky);
	counterLCD++;
}

void BasicTimer4_Callback(void){
	counterSampling++;
}
/*	=	=	=	FIN DE LAS RUTINAS DE ATENCIÓN (Callbacks)	=	=	=	*/