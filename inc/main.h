#ifndef __MAIN_H
#define __MAIN_H

void setup(void);
void LCDdisplayInit(int version);
void TIM2_IRQHandler(void);
void ADC_IRQHandler(void);

#endif /* __MAIN_H */
