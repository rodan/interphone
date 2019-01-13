#ifndef __PROJ_H__
#define __PROJ_H__

#include <msp430.h>
#include <stdlib.h>
#include <inttypes.h>
#include "config.h"

#define open_enable         P1OUT |= BIT4
#define open_disable        P1OUT &= ~BIT4
#define talk_enable         P1OUT |= BIT5
#define talk_disable        P1OUT &= ~BIT5

#define true                1
#define false               0

void main_init(void);
void wake_up(void);
void check_events(void);
void open_door(void);

#endif
