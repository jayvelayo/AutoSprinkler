#include <msp430.h>


// variable initialization
unsigned int ADCresults[8]; //store ADC results here
unsigned int plant1value = 0; //Analog value at P1.7
unsigned int plant2value = 0; //                P1.6


// Threshold values for each plants
unsigned int const plant1DryValue = 800;
unsigned int const plant1WetValue = 350;
unsigned int const plant2DryValue = 800;
unsigned int const plant2WetValue = 625;

void readSensorValues(void) {
    ADC10CTL0 &= ~ENC;
    while (ADC10CTL1 & ADC10BUSY);               // Wait if ADC10 core is active
    ADC10SA = (int)ADCresults;                   // Data buffer start
    ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion ready
    __bis_SR_register(CPUOFF + GIE);        // LPM0, ADC10_ISR will force exit
}

int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    /*
     * Clock Set up
     * MCLK / SMCLK = DCO = 1MHz
     * ACLK = XT1 = 32.768kHz
     */
    if (CALBC1_1MHZ==0xFF)                    // If calibration constant erased
    {
      while(1);                               // do not load, trap CPU!!
    }
    DCOCTL = 0;                               // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                    // Set range
    DCOCTL = CALDCO_1MHZ;                     // Set DCO step + modulation */
    BCSCTL1 |= XT2OFF + DIVA_3;              // ACLK/8
    BCSCTL2 = 0x00;
    BCSCTL3 |= XCAP_3;                      //12.5pF cap- setting for 32768Hz crystal

    /*
     * TA0 Clock set up for 1 min intervals
     */
    TA0CCTL0 = CCIE;                   // CCR0 interrupt enabled
    TA0CCR0 = 511;                 // 512 -> 1 sec, 30720 -> 1 min
    TA0CTL = TASSEL_1 + ID_3 + MC_1;         // ACLK, /8, upmode

    /*
     * TA1 Clock setup for 50Hz PWM
     */
    //set P2.1 to TA1.1 mode
    P2DIR = 0xFF; //set all P2 to output
    P2OUT = 0x00; //turn all off
    P2SEL |= BIT1;
    P2REN &= ~BIT1; //turn on pullup resistor for unused pins

    //set timer A1 registers
    TA1CCR0 = 20000-1;
    TA1CCTL1 = OUTMOD_7;
    TA1CCR1 = 500;
    TA1CTL = TASSEL_2 + MC_1; //set SMCLK for TIMER A source, UP mode7


    /*
     * ADC10 set up
     */
    ADC10CTL1 = INCH_7 + CONSEQ_1;            // A7 to A0, repeat multi channel
    ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON + ADC10IE;
    ADC10AE0 = BIT6 + BIT7;                          // P1.6,7 ADC option select
    ADC10DTC1 = 8;                         // 8 conversions

    /*
     * Port1 set up
     */
    P1DIR = 0x3F; //set everything except P1.6/7 to output mode
    P1OUT = 0x00; //turn off everything
    P1REN |= BIT1+BIT2; //enable pull up for unused ports

    while(1) {
        //turn on sensor and LED
        P1OUT = BIT4+BIT5+BIT0;
        __delay_cycles(1000000); //wait 1sec for sensor to settle
        readSensorValues();

        // Water plant 1 if needed
        if (plant1value > plant1DryValue){
            //move servo to position
            TA1CCR1 = 500;
            __delay_cycles(1000000); //allow servo to move
            P1OUT |= BIT3; //turn on pump;
            while(plant1value >= plant1WetValue) {
                readSensorValues();
            }
            P1OUT &= ~BIT3; // turn off pump
        }

        // Water plant 2 if needed
        if (plant2value > plant2DryValue){
            //move servo to position
            TA1CCR1 = 2300;
            __delay_cycles(1000000); //allow servo to move
            P1OUT |= BIT3; //turn on pump;
            while(plant2value >= plant2WetValue) {
                readSensorValues();
            }
            P1OUT &= ~BIT3; // turn off pump
        }

        //turn sensor and LED off
        P1OUT &= ~(BIT4+BIT5+BIT0);

        _BIS_SR(LPM3_bits + GIE);       // Enter LPM3, interrupts enabled
    }
}


// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
    static unsigned int currentMinutes = 0;
    currentMinutes++;

    P1OUT ^= BIT0;

    //stay asleep until 1 day has passed
    if (currentMinutes == 1440) {  //1440 minutes = 1 day
        currentMinutes = 0;
        __bic_SR_register_on_exit(LPM3_bits);
    }
}

// ADC10 Interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
    plant1value = ADCresults[0];
    plant2value = ADCresults[1];
  __bic_SR_register_on_exit(CPUOFF);        // Clear CPUOFF bit from 0(SR)
}
