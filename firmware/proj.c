
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
#include "proj.h"
#include "drivers/sys_messagebus.h"
#include "drivers/timer_a0.h"

#ifdef CONFIG_DEBUG
#include "drivers/uart1.h"
#endif

// the period in which a second trigger is expected, in timer A0 ticks.
// ticks = time(sec) * 32768
#define period_min 120000
#define period_max 150000

// an integer representing the time after which the uC will go into LPM4 standby
// standby_time(sec) = standby_time(int) * 2sec
// (2sec is the overflow time of TA0R)
#define standby_time 10

// TRIG1 is the 9V input trigger (interphone cable 'PX')
// tied to P1.1
#define TRIG1 BIT1

volatile uint8_t trigger1;
uint32_t last_trigger;
char str_temp[64];

static void timer_a0_irq(enum sys_message msg)
{
    if (timer_a0_ovf >= standby_time) {
        // disable TA0 so that LPM4 can have maximum effectiveness
        TA0CTL = 0;
#ifdef USE_WATCHDOG
        WDTCTL = WDTPW + WDTHOLD;
#endif
        _BIS_SR(LPM4_bits + GIE);
        __no_operation();

        // a PORT1 irq just came in, so we wake up
        timer_a0_init();
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif
        trigger1 = false;
        last_trigger = 0;
    }
}

int main(void)
{
    main_init();
#ifdef CONFIG_DEBUG
    uart1_init();
#endif

    sys_messagebus_register(&timer_a0_irq, SYS_MSG_TIMER0_IFG);

    while (1) {
        // sleep
        _BIS_SR(LPM3_bits + GIE);
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
    // watchdog triggers after 25sec when not cleared
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#else
    WDTCTL = WDTPW + WDTHOLD;
#endif
    P5SEL |= BIT5 + BIT4;

    P1SEL = 0;
    P1DIR = 0xfd;
    P1OUT = 0;

    // IRQ triggers on rising edge
    P1IES &= ~TRIG1;
    // Reset IRQ flags
    P1IFG &= ~TRIG1;
    // Enable button interrupts
    P1IE |= TRIG1;

    P2SEL = 0;
    P2DIR = 0x1;
    P2OUT = 0;

    P3SEL = 0;
    P3DIR = 0x1f;
    P3OUT = 0;

    P4SEL = 0;
    P4DIR = 0xff;
    P4OUT = 0;

    //P5SEL is set above
    P5DIR = 0xff;
    P5OUT = 0;

    P6DIR = 0xff;
    P6OUT = 0;

    PJDIR = 0xff;
    PJOUT = 0;

    // disable VUSB LDO and SLDO
    USBKEYPID = 0x9628;
    USBPWRCTL &= ~(SLDOEN + VUSBEN);
    USBKEYPID = 0x9600;

    timer_a0_init();
    trigger1 = false;
    last_trigger = 0;
}

void wake_up(void)
{
    uint32_t trig_time, trig_diff;
    if (trigger1) {
        // debounce
        timer_a0_delay(50000);
        if ((P1IN & TRIG1) == 0) {
            return;
        } else {
            trig_time = ((uint32_t) timer_a0_ovf << 16) + (uint16_t) TA0R;
            trig_diff = trig_time - last_trigger;
#ifdef CONFIG_DEBUG
            sprintf(str_temp, "trig1 %ld %ld\r\n", trig_time, trig_diff);
            uart1_tx_str(str_temp, strlen(str_temp));
#endif
            if ((trig_diff > period_min) && (trig_diff < period_max)) {
                P1IE &= ~TRIG1;
#ifdef CONFIG_DEBUG
                snprintf(str_temp, 7, "open\r\n");
                uart1_tx_str(str_temp, strlen(str_temp));
#endif
                open_door();
                P1IE |= TRIG1;
            }
            last_trigger = trig_time;
        }
        trigger1 = false;
    }
}

void check_events(void)
{
    struct sys_messagebus *p = messagebus;
    enum sys_message msg = 0;

    // drivers/timer_a0
    if (timer_a0_last_event) {
        msg |= timer_a0_last_event << 7;
        timer_a0_last_event = 0;
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
    timer_a0_delay(100000);
    talk_enable;
    timer_a0_delay(100000);
    talk_disable;
    timer_a0_delay(100000);
    open_enable;
    timer_a0_delay(100000);
    open_disable;
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    if (P1IFG & TRIG1) {
        trigger1 = true;
        P1IFG &= ~TRIG1;
        LPM4_EXIT;
    }
}

