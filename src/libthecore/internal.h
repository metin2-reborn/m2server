// State and functions shared between libthecore's sources only.
#ifndef __INC_LIBTHECORE_INTERNAL_H__
#define __INC_LIBTHECORE_INTERNAL_H__

#include <libthecore/heart.h>

#include <csignal>

// Written from signal handlers.
extern volatile std::sig_atomic_t tics;
extern volatile std::sig_atomic_t shutdowned;

void	signal_setup();

bool	log_init();
void	log_destroy();

heart *	heart_new(int fps);
int	heart_idle(heart * ht);	// Returns how many pulses have passed.

#endif
