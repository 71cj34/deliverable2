#include <stdint.h>

#include "PLL.h"

#include "SysTick.h"

#include "uart.h"

#include "onboardLEDs.h"

#include "tm4c1294ncpdt.h"

#include "VL53L1X_api.h"

// CONNECT OSCILLOSCOPE ON PM4 FOR CLOCK DEMO
// CHANGELOG FOR CLOCK CYCLE STUFF:
// changed systick to use 26 mult
// changed i2c clock speed
// changed timer3_init

#define I2C_MCS_ACK 0x00000008 // Data Acknowledge Enable
#define I2C_MCS_DATACK 0x00000008 // Acknowledge Data
#define I2C_MCS_ADRACK 0x00000004 // Acknowledge Address
#define I2C_MCS_STOP 0x00000004 // Generate STOP
#define I2C_MCS_START 0x00000002 // Generate START
#define I2C_MCS_ERROR 0x00000002 // Error
#define I2C_MCS_RUN 0x00000001 // I2C Master Enable
#define I2C_MCS_BUSY 0x00000001 // I2C Busy
#define I2C_MCR_MFE 0x00000010 // I2C Master Function Enable

#define MAXRETRIES 5 // number of receive attempts before giving up
void I2C_Init(void) {
    SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R0; // activate I2C0
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1; // activate port B
    while ((SYSCTL_PRGPIO_R & 0x0002) == 0) {}; // ready?

    GPIO_PORTB_AFSEL_R |= 0x0C; // 3) enable alt funct on PB2,3       0b00001100
    GPIO_PORTB_ODR_R |= 0x08; // 4) enable open drain on PB3 only

    GPIO_PORTB_DEN_R |= 0x0C; // 5) enable digital I/O on PB2,3
    //    GPIO_PORTB_AMSEL_R &= ~0x0C;          																// 7) disable analog functionality on PB2,3

    // 6) configure PB2,3 as I2C
    //  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFF00FF)+0x00003300;
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xFFFF00FF) + 0x00002200; //TED
    I2C0_MCR_R = I2C_MCR_MFE; // 9) master function enable
			I2C0_MTPR_R = 13; // 8) configure for 100 kbps clock (added 8 clocks of glitch suppression ~50ns)      // CHANGED: TPR = (clk/(2*(delay+1)*100000)) - 1, (26m/(2*10*100000)) - 1 ~= 13
    //    I2C0_MTPR_R = 0x3B;                                        						// 8) configure for 100 kbps clock

}

//The VL53L1X needs to be reset using XSHUT.  We will use PG0
void PortG_Init(void) {
    //Use PortG0
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R6; // activate clock for Port N
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R6) == 0) {}; // allow time for clock to stabilize
    GPIO_PORTG_DIR_R &= 0x00; // make PG0 in (HiZ)
    GPIO_PORTG_AFSEL_R &= ~0x01; // disable alt funct on PG0
    GPIO_PORTG_DEN_R |= 0x01; // enable digital I/O on PG0
    // configure PG0 as GPIO
    //GPIO_PORTN_PCTL_R = (GPIO_PORTN_PCTL_R&0xFFFFFF00)+0x00000000;
    GPIO_PORTG_AMSEL_R &= ~0x01; // disable analog functionality on PN0

    return;
}

void PortM_Init(void) {
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R11; // Activate the clock for Port E
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R11) == 0) {}; // Allow time for clock to stabilize

    GPIO_PORTM_DIR_R = 0b00011111; // Enable PE0 and PE1 as outputs
    GPIO_PORTM_AFSEL_R &= ~0b00011111;
    GPIO_PORTM_DEN_R = 0b00011111; // Enable PE0 and PE1 as digital pins
    GPIO_PORTM_AMSEL_R &= ~0b00011111;
    return;
}

//XSHUT     This pin is an active-low shutdown input;
//					the board pulls it up to VDD to enable the sensor by default.
//					Driving this pin low puts the sensor into hardware standby. This input is not level-shifted.
void VL53L1X_XSHUT(void) {
    GPIO_PORTG_DIR_R |= 0x01; // make PG0 out
    GPIO_PORTG_DATA_R &= 0b11111110; //PG0 = 0
    FlashAllLEDs();
    SysTick_Wait10ms(10);
    GPIO_PORTG_DIR_R &= ~0x01; // make PG0 input (HiZ)

}

static int step_index = 0; // Persistent variable
static int le = 0;
static int dir = 1;
void step_motor(int dir) {
    int seq[4] = {0b0011, 0b0110, 0b1100, 0b1001};
    
    // Perform only one step
    GPIO_PORTM_DATA_R = seq[step_index % 4];
		le += 1;
    
    // Increment or decrement index
    if (dir == 0) step_index = (step_index == 0) ? 3 : step_index - 1;
    else step_index = (step_index + 1) % 4;
}

void EnableInt(void)
{    __asm("    cpsie   i\n");
}

// Disable interrupts
void DisableInt(void)
{    __asm("    cpsid   i\n");
}

// Low power wait
void WaitForInt(void)
{    __asm("    wfi\n");
}

void Timer3_Init(void){

	uint32_t period = 820000;						// 32-bit value in 1us increments
	
	// Step 1: Activate timer
	SYSCTL_RCGCTIMER_R = 0x08;				// (Step 1)Activate timer 
	SysTick_Wait10ms(1);							// Wait for the timer module to turn on
	
	
	// Step 2: Arm and Configure Timer Module
	TIMER3_CTL_R = 0x0;						// (Step 2-1) Disable Timer3 during setup (Timer stops counting)
	TIMER3_CFG_R = 0x0;						// (Step 2-2) Configure for 32-bit timer mode   
	TIMER3_TAMR_R = 0x2;						// (Step 2-3) Configure for periodic mode   
	TIMER3_TAPR_R = 0x0;						// (Step 2-4) Set prescale value to 0; i.e. Timer3 works with Maximum Freq = bus clock freq (120MHz)  
	TIMER3_TAILR_R = (period*26)-1; 			// (Step 2-5) Reload value (we multiply the period by 26 to match the units of 1 us)  
	TIMER3_ICR_R = 0x1;							// (Step 2-6) Acknowledge the timeout interrupt (Clear timeout flag of Timer3)
	TIMER3_IMR_R = 0x1;						// (Step 2-7) Arm timeout interrupt   
	
	
	// Step 3: Enable Interrupt at Processor side
	NVIC_EN1_R = 0x00000008;					// Enable IRQ 35 in NVIC 
	NVIC_PRI8_R = 0x40000000;					// Set Interrupt Priority to 2 
																
	EnableInt();									// Global Interrupt Enable
	
	
	// Step 4: Enable the Timer to start counting
	TIMER3_CTL_R = 0x1;						// Enable Timer3
} 

void PortJ_Init(void) {
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R8; // Activate the clock for Port M
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R8) == 0) {}; // Allow time for clock to stabilize

    GPIO_PORTJ_LOCK_R = 0x4C4F434B;
    GPIO_PORTJ_CR_R |= 0b011;
    GPIO_PORTJ_LOCK_R = 0;

    GPIO_PORTJ_DIR_R = 0b00000000; // Enable PM0 and PM1 as inputs
    GPIO_PORTJ_DEN_R = 0b00000011; // Enable PM0 and PM1 as digital pins

    // pull down resistors for active-high buttons
    GPIO_PORTJ_PUR_R = 0b00000011;
    return;
}

//*********************************************************************************************************
//*********************************************************************************************************
//***********					MAIN Function				*****************************************************************
//*********************************************************************************************************
//*********************************************************************************************************
uint16_t dev = 0x29; //address of the ToF sensor as an I2C slave peripheral
int status = 0;

void TIMER3A_IRQHandler(void){ 
	  TIMER3_ICR_R = 0x01; // Acknowledge Timer0A interrupt
    
    uint8_t dataReady = 0;
    uint16_t distance;
		FlashLED4(1);
    
    // Check if the sensor has finished the previous measurement
    VL53L1X_CheckForDataReady(dev, &dataReady);
    
    if (dataReady) {
        VL53L1X_GetDistance(dev, &distance);
        VL53L1X_ClearInterrupt(dev); 
        
        // Log it
        sprintf(printf_buffer, "%u\r\n", distance);
				FlashLED4(1);
        UART_printf(printf_buffer);
        
        // Trigger the next ranging capture immediately
        // (If not already running in continuous mode)
    }

}

void MeasureOnce(void) {
	        status = VL53L1X_StartRanging(dev);   // 4 This function has to be called to enable the ranging

        while (le < 2048) {
					if (le % 512 == 0) {
								FlashLED3(1);
					}
					step_motor(dir);
					SysTick_Wait10us(1000);
				}

        VL53L1X_StopRanging(dev);
}

int main(void) {
    uint8_t byteData, sensorState = 0, myByteArray[10] = {
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF
    }, i = 0;
    uint16_t wordData;
    uint16_t Distance;
    uint16_t SignalRate;
    uint16_t AmbientRate;
    uint16_t SpadNum;
    uint8_t RangeStatus;
    uint8_t dataReady;

    //initialize
    PLL_Init();
    SysTick_Init();
    onboardLEDs_Init();
    I2C_Init();
    UART_Init();
		PortM_Init();
		Timer3_Init();
		PortJ_Init();
		
    // hello world!
//    //UART_printf("Program Begins\r\n");
    int mynumber = 1;
    //sprintf(printf_buffer, "2DX ToF Program Studio Code %d\r\n", mynumber);
//    //UART_printf(printf_buffer);

    /* Those basic I2C read functions can be used to check your own I2C functions */
    status = VL53L1X_GetSensorId(dev, & wordData);

    //sprintf(printf_buffer, "(Model_ID, Module_Type)=0x%x\r\n", wordData);
//    //UART_printf(printf_buffer);

    // 1 Wait for device ToF booted
    while (sensorState == 0) {
        status = VL53L1X_BootState(dev, & sensorState);
        SysTick_Wait10ms(10);
    }
    FlashAllLEDs();
    //UART_printf("ToF Chip Booted!\r\n Please Wait...\r\n");

    status = VL53L1X_ClearInterrupt(dev); /* clear interrupt has to be called to enable next interrupt*/

    // CUSTOM STUFF MILESTONE 1

    uint8_t modelID = 0;
    uint8_t module = 0;
    uint16_t both = 0;

    //UART_printf("TRYING SENSOR INFO...\r\n");

    status = VL53L1_RdByte(dev, 0x010F, &modelID);
    if(status == 0) {
        //sprintf(printf_buffer, "Model ID (Byte): 0x%02X\r\n", modelID);
        //UART_printf(printf_buffer);
    }

    status = VL53L1X_ClearInterrupt(dev);

    status = VL53L1_RdByte(dev, 0x0110, &module);
    if(status == 0) {
        //sprintf(printf_buffer, "Module Type (Byte): 0x%02X\r\n", module);
        //UART_printf(printf_buffer);
    }

    status = VL53L1X_ClearInterrupt(dev);

    status = VL53L1_RdWord(dev, 0x010F, &both);
    if(status == 0) {
        //sprintf(printf_buffer, "Combined WordData: 0x%04X\r\n", both);
        //UART_printf(printf_buffer);
    }

    status = VL53L1X_ClearInterrupt(dev);

    // END CUSTOM STUFF MILESTONE 1

    /* 2 Initialize the sensor with the default setting  */
    status = VL53L1X_SensorInit(dev);
    Status_Check("SensorInit", status);

    /* 3 Optional functions to be used to change the main ranging parameters according the application requirements to get the best ranging performances */
    //  status = VL53L1X_SetDistanceMode(dev, 2); /* 1=short, 2=long */
    status = VL53L1X_SetTimingBudgetInMs(dev, 50); /* in ms possible values [20, 50, 100, 200, 500] */
    //  status = VL53L1X_SetInterMeasurementInMs(dev, 200); /* in ms, IM must be > = TB */

    ///////////////////// MILESTONE 2 - UNCOMMENT THIS TO DO M2!!!!
		int pressed = 0;
		int last = 0;
    if (1) {
			pressed = 0;
			int current = GPIO_PORTJ_DATA_R;
			pressed = current & (~last);
			last = current;
			
			dir ^= 1;
			if (pressed & 0b01) {
				MeasureOnce();
			}
    } else {			
			while (1) {
				GPIO_PORTM_DATA_R ^= (1 << 4);
				SysTick_Wait(100000);
		}
		}
}
