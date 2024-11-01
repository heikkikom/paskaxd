/* C Standard library */
#include <stdio.h>
#include <string.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// JTKJ: Exercise 3. Definition of the state machine
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

// JTKJ: Exercise 3. Global variable for ambient light
double ambientLight = -1000.0;

// JTKJ: Exercise 1. Add pins RTOS-variables and configuration here
// Added from Lovelace material 15

static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    // JTKJ: Exercise 1. Blink either led of the device
    uint_t pinValue = PIN_getOutputValue(Board_LED1);
      pinValue = !pinValue;
      PIN_setOutputValue(ledHandle, Board_LED1, pinValue);
}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {


       // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1
       UART_Handle uart;
       UART_Params uartParams;

       // Initialize serial communication
       UART_Params_init(&uartParams);
       uartParams.writeDataMode = UART_DATA_TEXT;
       uartParams.readDataMode = UART_DATA_TEXT;
       uartParams.readEcho = UART_ECHO_OFF;
       uartParams.readMode = UART_MODE_BLOCKING;
       uartParams.baudRate = 9600; // 9600 baud rate
       uartParams.dataLength = UART_LEN_8; // 8
       uartParams.parityType = UART_PAR_NONE; // n
       uartParams.stopBits = UART_STOP_ONE; // 1

       uart = UART_open(Board_UART0, &uartParams);
          if (uart == NULL) {
             System_abort("Error opening the UART");
          }

    while (1) {

        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state

        if (programState == DATA_READY){
            char tulos[20];
            sprintf(tulos, "Valoisuus: %.2f\n\r", ambientLight);
            UART_write(uart, tulos, strlen(tulos));
            programState = WAITING;
        }

        /* sprintf(tulos, "Tulos: %.2f ", ambientLight);
           System_printf("s%", tulos);
           System_flush();  **/

        // Once per 100ms
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {

    I2C_Handle      i2c;
    I2C_Params      i2cParams;

    // JTKJ: Exercise 2. Open the i2c bus
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
       System_abort("Error Initializing I2C\n");
    }

    // JTKJ: Exercise 2. Setup the OPT3001 sensor for use
    //       Before calling the setup function, insertt 100ms delay with Task_sleep
    Task_sleep(100000 / Clock_tickPeriod);

    opt3001_setup(&i2c);

    while (1) {

        if (programState == WAITING) {
            ambientLight = opt3001_get_data(&i2c);
            programState = DATA_READY;
        }

        // JTKJ: Exercise 2. Read sensor data and print it to the Debug window as string
        // JTKJ: Exercise 3. Save the sensor value into the global variable
        //       Remember to modify state

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Int main(void) {

    // Task variables
    // Lisättiin lovelace materiaalista 15
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    // JTKJ: Exercise 2. Initialize i2c bus
    Board_initI2C();

    // JTKJ: Exercise 4. Initialize UART
    Board_initUART();

    // JTKJ: Exercise 1. Open the button and led pins
    //       Remember to register the above interrupt handler for button

    // Initialize the LED in the program
    // Added from lovelace material 15

    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle) {
       System_abort("Error initializing LED pin\n");
    }

    // Initialize the button in the program
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle) {
       System_abort("Error initializing button pin\n");
    }

    // Register the button interrupt handler
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
