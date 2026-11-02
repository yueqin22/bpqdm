
#ifndef _SFO_V6_H
#define _SFO_V6_H

#ifdef __cplusplus
extern "C" {
#endif

#define PWM_CH  5

int SFO(void);

#define SFO_INCOMPLETE      0
#define SFO_COMPLETE        1
#define SFO_ERROR           2

#endif

#ifdef __cplusplus
}
#endif
