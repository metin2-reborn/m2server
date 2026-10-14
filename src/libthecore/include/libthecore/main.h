#ifndef __INC_LIBTHECORE_MAIN_H__
#define __INC_LIBTHECORE_MAIN_H__

#include <libthecore/heart.h>

extern heart *	thecore_heart;

extern unsigned int	thecore_idle_time;	// ms spent waiting for pulses; the caller resets it

bool	thecore_init(int fps);
int	thecore_idle();
void	thecore_shutdown();
void	thecore_destroy();
int	thecore_pulse();
bool	thecore_is_shutdowned();
void	thecore_tick(); // Increase tics

#endif
