
//*******************************************************************************
// To implement breathing function,Need to move the Pin Low duration from High
// to low and low to high continuously. Can use Time Capture compare module to
// Set Pin high low functions with timer interrupt, which will handle all the
// sequencing. In breathing The pin will never go to totally off, but kept at
// minimum defined value from full brightness, which I chosen 10%.
//
// Functional requirements
// 1. From 10%->~100% PWM within 1.8s, ~100%->10% within 1.8s
// 2. No of brightness levels 200
//
// SMCLK = 1MHz -> Timer0 Interrupt @ each 1us
// CCR0 = 1000 gives, 1 set/reset by capture compare
// within 1ms, which gives PWM Frequency of 1kHz.
// Therefore to complete 100 to 1000 CCR1 scale during 1.8s, 900 steps will
// change over 1800ms, once in 2ms CCR1 is to be changed.
// The divisor = 2000, Step = 1

// Implementation
// 1. When PIN P1.0 is high LED is off
// 2. When PIN P1.0 is low LED is fully on
// 3. Set the Pin to output mode &  wire to controlled by capture compare module
// 4. SMCLK is set to feed TACLK, SMCLK@1MHz, Timer Interrupt fire for 1us.
// 5. Initialize the timer in up mode with its CCR modules & enable interrupt
// 6. In Timer interrupt keep track of CCR0 value and step change accordingly
// 7. Within he ISR, control the PWM duty cycle by changing CCR1 set value, to cycle between 99%->10%
//
//!
//!  MSP430F5529
//!            -------------------
//!           |                   |
//!           |                   |
//!           |                   |
//!           |         P1.0/TA1.1|--> CCR1 BREATHING PWM
//!
//*****************************************************************************

#include "driverlib.h"
#include "msp430.h"

#define DIV_INTERVAL 2000
#define STEP 1
#define LOWEST_PWM 100
#define HIGHEST_PWM 1000
#define DOWN_CNT 1
#define UP_CNT 2



void init(){

    /* Stop Watchdog */
    WDTCTL = WDTPW + WDTHOLD;

    /*To set P1.0 as Output the following register values has to set */
    P1DIR |= BIT0;
    P1SEL |= BIT0;

    /* Set the PWM period = 1MHz / 1023 = 960 Hz */
    TA0CCR0 = HIGHEST_PWM;

    /* Source Timer A from SMCLK (TASSEL_2), up mode (MC_1)
     * Up mode counts up to TA0CCR0 till 1000 in 1ms
     * Therefore, PWM frequency is 1kHz
     * Set Out Mode 7 to let Set and Reset the Pin
     * Enable Interrupt when Timer A count up to TACCR & handle CCR1
     * cyclic scaling from 100->1000->100...
     * To let 100->1000 rise during 1.8seconds, 900 steps in 1,800,000 interrupts
     * One CCR1 step up or down for 2000 timer interrupts
     */
    TA0CTL = TASSEL_2 | MC_1;
    TA0CCTL1 = OUTMOD_7 | CCIE;

     /*Set initial CCR1 */
    TA0CCR1 = LOWEST_PWM;
}

void main (void)
{

    init();

    /*Enter Low power mode 3 with interrupts enabled*/
    __bis_SR_register(LPM3_bits);

}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void Timer0()
{
    /*Each 1us interval this interrupt fires. We process the logic once
     * in 2000 intervals set by DIV_INTERVAL. once in 2ms, we inc/dec
     * CCR1, and eventually in 1800ms = 1.8s, one direction is completed
     * within 3.6s, One breathing cycle is completed.
     */


    /*Define some static variables to help to change the direction
     * and the timer division
     */
    static int dir = DOWN_CNT;       //Initial count direction
    static int div = 0;              //Count the number of timer interrupts to use

    if(div >= DIV_INTERVAL){

        if(TA0CCR1 > LOWEST_PWM && dir == DOWN_CNT){
            TA0CCR1 -= STEP;
        }else if(TA0CCR1 <= HIGHEST_PWM && dir == UP_CNT){
            TA0CCR1 += STEP;
        }

        /*Cycle the PWM values*/
        /*When hit the Lowest PWM, start count up*/
        if (TA0CCR1 == LOWEST_PWM)
            dir = UP_CNT;

        /*When hit the highest PWM, start count down*/
        if (TA0CCR1 == HIGHEST_PWM)
            dir = DOWN_CNT;

        div = 0; //reset divisor

    }else{

        div++; //keep increment the divisor get visible frequency fro breathing
    }
 }

//void main (void)
//{
//
//    init();
//    /*Stop with WDT WDTCTL = WDTPW | WDTHOLD;
//    * The below library function in WDT_a.c simply handle
//    * this as below
//    * */
//    WDT_A_hold(WDT_A_BASE);
//
//    /*To set P1.0 as Output & controlled by CCP module,
//     * the following register values has to set
//     * P1DIR |= BIT0;
//     * P1SEL |= BIT0;
//     * The below library function from GPIO.c is used to implement
//     */
//
//    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1,GPIO_PIN0);
//
//    /*To implement breathing function, The*/
//    Timer_A_initUpModeParam initUpParam = {0};
//    initUpParam.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
//    initUpParam.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
//    initUpParam.timerPeriod = TIMER_PERIOD;
//    initUpParam.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;
//    initUpParam.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE;
//    initUpParam.timerClear = TIMER_A_DO_CLEAR;
//    initUpParam.startTimer = false;
//    Timer_A_initUpMode(TIMER_A1_BASE, &initUpParam);
//
//    Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);
//
//    //Initialize compare mode to generate PWM1
//    Timer_A_initCompareModeParam initComp1Param = {0};
//    initComp1Param.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
//    initComp1Param.compareInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE;
//    initComp1Param.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
//    initComp1Param.compareValue = DUTY_CYCLE1;
//    Timer_A_initCompareMode(TIMER_A1_BASE, &initComp1Param);
//
//
//    /*Enter Low power mode 3 with interrupts enabled*/
//    __bis_SR_register(LPM3_bits);
//
//
//    return 0;
//}
