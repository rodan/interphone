#ifndef __SC_H__
#define __SC_H__

#include <msp430.h>
#include <stdlib.h>
#include "config.h"

#define open_enable         P1OUT |= BIT4
#define open_disable        P1OUT &= ~BIT4
#define talk_enable         P1OUT |= BIT5
#define talk_disable        P1OUT &= ~BIT5
#define r_enable            P1OUT |= BIT7
#define r_disable           P1OUT &= ~BIT7

#define MCLK_FREQ           1048576

#define true                1
#define false               0

void main_init(void);
void sleep(void);
void wake_up(void);
void check_events(void);
void open_door(void);

#endif
