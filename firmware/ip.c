
//  MSP430F5510 based project that ties into the appartment interphone system. 
//  in this case it's an Electra IA01.
//  it allows the owner outside the building to open the door by following a 
//  pre-determined list of actions.
//
//  author:          Petre Rodan <petre.rodan@simplex.ro>
//  available from:  https://github.com/rodan/
//  license:         GNU GPLv3

#include <stdio.h>
#include <string.h>

#include "ip.h"
#include "calib.h"
#include "drivers/sys_messagebus.h"
#include "drivers/pmm.h"
#include "drivers/rtc.h"
#include "drivers/timer_a0.h"
#include "drivers/uart.h"
#include "drivers/adc.h"

volatile uint8_t trigger1;

// TRIG1 is the 9V input trigger (interphone cable 'PX')
// tied to P1.1
#define TRIG1 BIT1

#ifdef RADIO
// TRIG2 is the radio control trigger. active low.
// tied to P1.2
#define TRIG2 BIT2
volatile uint8_t trigger2;
#endif

uint32_t last_trigger;

char str_temp[64];

#ifdef CALIBRATION
static void do_calib(enum sys_message msg)
{
    uint16_t q_bat = 0;
    float v_bat;

    adc10_read(0, &q_bat, REFVSEL_2);
    v_bat = q_bat * VREF_2_5 * DIV_BAT;
    adc10_halt();

    //1023 22.22DA0
    snprintf(str_temp, 13, "%4d %02d.%02d\r\n",
             q_bat, (uint16_t) v_bat / 100, (uint16_t) v_bat % 100);
    uart_tx_str(str_temp, strlen(str_temp));

}
#endif                          // CALIBRATION

int main(void)
{
    main_init();
    //uart_init();

#ifdef CALIBRATION
    sys_messagebus_register(&do_calib, SYS_MSG_RTC_SECOND);
#endif

    //snprintf(str_temp, 4, "?\r\n");
    //uart_tx_str(str_temp, strlen(str_temp));

    while (1) {
        sleep();
        __no_operation();
        wake_up();
#ifdef USE_WATCHDOG
        WDTCTL = (WDTCTL & 0xff) | WDTPW | WDTCNTCL;
#endif
        check_events();
    }
}

void main_init(void)
{
    //uint16_t timeout = 5000;

    // watchdog triggers after 4 minutes when not cleared
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__8192K + WDTSSEL__ACLK + WDTCNTCL;
#else
    WDTCTL = WDTPW + WDTHOLD;
#endif
    SetVCore(3);

    P5SEL |= BIT5 + BIT4;

    P1SEL = 0x0;
    P1DIR = 0xf9;
    P1OUT = 0x00;

    // IRQ triggers on rising edge
    P1IES &= ~TRIG1;
    // Reset IRQ flags
    P1IFG &= ~TRIG1;
    // Enable button interrupts
    P1IE |= TRIG1;

#ifdef RADIO
    P1REN |= TRIG2;  // enable internal resistance
    P1OUT |= TRIG2;  // set as pull-up resistance
    P1IES |= TRIG2;  // Hi/Lo edge
    P1IFG &= ~TRIG2; // IFG cleared
    trigger2 = false;
#endif

    P2SEL = 0x0;
    P2DIR = 0x1;
    P2OUT = 0x0;

    P3SEL = 0x0;
    P3DIR = 0x1f;
    P3OUT = 0x0;

    P4SEL = 0x00;
    P4DIR = 0xb9;
    P4OUT = 0x0;

    //P5SEL is set above
    P5DIR = 0x0;
    P5OUT = 0x0;

    P6SEL = 0x1;
    P6DIR = 0x00;
    P6REN = 0x0a;

    PJOUT = 0x00;
    PJDIR = 0xFF;

#ifdef CALIBRATION
    // send MCLK to P4.0
    __disable_interrupt();
    // get write-access to port mapping registers
    //PMAPPWD = 0x02D52;
    PMAPPWD = PMAPKEY;
    PMAPCTL = PMAPRECFG;
    // MCLK set out to 4.0
    P4MAP0 = PM_MCLK;
    //P4MAP0 = PM_RTCCLK;
    PMAPPWD = 0;
    __enable_interrupt();
    P4DIR |= BIT0;
    P4SEL |= BIT0;

    // send ACLK to P1.0
    P1DIR |= BIT0;
    P1SEL |= BIT0;
#endif

    rtca_init();
    timer_a0_init();
    trigger1 = false;
    last_trigger = -1L;
}

void sleep(void)
{
    // disable VUSB LDO and SLDO
    USBKEYPID = 0x9628;
    USBPWRCTL &= ~(SLDOEN + VUSBEN);
    USBKEYPID = 0x9600;
    _BIS_SR(LPM3_bits + GIE);
    __no_operation();
}

void wake_up(void)
{

#ifdef RADIO
    uint8_t tries = 0;
    uint16_t q_bat = 0;
    float v_bat;
    adc10_read(0, &q_bat, REFVSEL_2);
    v_bat = q_bat * VREF_2_5 * DIV_BAT;
    adc10_halt();

    if (trigger1 && (v_bat > 1200)) {
        snprintf(str_temp, 13, "trig1 high\r\n");
        uart_tx_str(str_temp, strlen(str_temp));
        r_enable;
        timer_a0_delay(100000);
        P1IFG &= ~TRIG1;
        P1IE &= ~TRIG1;
        //trigger2 = false;
        P1IFG &= ~TRIG2;
        P1IE |= TRIG2;
        timer_a0_delay_noblk(1000000);
        while (tries < 10) {
            if (timer_a0_last_event & TIMER_A0_EVENT_CCR2) {
                timer_a0_last_event &= ~TIMER_A0_EVENT_CCR2;
                timer_a0_delay_noblk(1000000);
                tries++;
                snprintf(str_temp, 7, "t %2d\r\n",tries);
                uart_tx_str(str_temp, strlen(str_temp));
            }
            if (trigger2) {
                // debounce
                timer_a0_delay(50000);
                if (P1IN & TRIG2) {
                    continue;
                }
                P1IE &= ~TRIG2;
                snprintf(str_temp, 7, "open\r\n");
                uart_tx_str(str_temp, strlen(str_temp));
                r_disable;
                open_door();
                break;
            }
        }
        r_disable;
        P1IFG &= ~TRIG2;
        P1IE &= ~TRIG2;
        trigger1 = false;
        trigger2 = false;
        P1IE |= TRIG1;
    } else
#endif
    if (trigger1) {
        // debounce
        timer_a0_delay(50000);
        if ((P1IN & TRIG1) == 0) {
            return;
        } else {
            //sprintf(str_temp, "trig1 %ld\r\n", rtca_time.sys);
            //uart_tx_str(str_temp, strlen(str_temp));
            if (rtca_time.sys - last_trigger < 5) {
                P1IE &= ~TRIG1;
                //snprintf(str_temp, 7, "open\r\n");
                //uart_tx_str(str_temp, strlen(str_temp));
                open_door();
                P1IE |= TRIG1;
            }
            last_trigger = rtca_time.sys;
        }
        trigger1 = false;
    }
}

void check_events(void)
{
    struct sys_messagebus *p = messagebus;
    enum sys_message msg = 0;

    // drivers/rtca
    if (rtca_last_event) {
        msg |= rtca_last_event;
        rtca_last_event = 0;
    }
    while (p) {
        // notify listener if he registered for any of these messages
        if (msg & p->listens) {
            p->fn(msg);
        }
        p = p->next;
    }
}

void open_door(void)
{
    // 0.2 0.1 0.1 0.1 is working
    //P4OUT |= BIT3; // orange led on
    timer_a0_delay(100000);
    talk_enable;
    timer_a0_delay(100000);
    //P4OUT &= ~BIT3; // orange led off
    talk_disable;
    timer_a0_delay(100000);
    //P4OUT |= BIT7; // green led on
    open_enable;
    timer_a0_delay(100000);
    //P4OUT &= ~(BIT3 + BIT7); // leds off
    open_disable;
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    if (P1IFG & TRIG1) {
        trigger1 = true;
        P1IFG &= ~TRIG1;
        _BIC_SR_IRQ(LPM3_bits);
    }

#ifdef RADIO
    else if (P1IFG & TRIG2) {
        trigger2 = true;
        P1IFG &= ~TRIG2;
        _BIC_SR_IRQ(LPM3_bits);
    }
#endif
}
