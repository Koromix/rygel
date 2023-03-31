/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2018 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __INTERVALTIMER_H__
#define __INTERVALTIMER_H__

#include <stddef.h>
#include "imxrt.h"

#ifdef __cplusplus
extern "C" {
#endif

// IntervalTimer provides access to hardware timers which can run an
// interrupt function a precise timing intervals.  Up to 4 IntervalTimers
// may be in use simultaneously.  Many libraries use IntervalTimer, so
// some of these 4 possible instances may be in use by libraries.
class IntervalTimer {
private:
	static const uint32_t MAX_PERIOD = UINT32_MAX / (24000000 / 1000000);
public:
	constexpr IntervalTimer() {
	}
	~IntervalTimer() {
		end();
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), unsigned int microseconds) {
		if (microseconds == 0 || microseconds > MAX_PERIOD) return false;
		uint32_t cycles = (24000000 / 1000000) * microseconds - 1;
		if (cycles < 17) return false;
		return beginCycles(funct, cycles);
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), int microseconds) {
		if (microseconds < 0) return false;
		return begin(funct, (unsigned int)microseconds);
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), unsigned long microseconds) {
		return begin(funct, (unsigned int)microseconds);
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), long microseconds) {
		return begin(funct, (int)microseconds);
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), float microseconds) {
		if (microseconds <= 0 || microseconds > MAX_PERIOD) return false;
		uint32_t cycles = (float)(24000000 / 1000000) * microseconds - 0.5f;
		if (cycles < 17) return false;
		return beginCycles(funct, cycles);
	}
	// Start the hardware timer and begin calling the function.  The
	// interval is specified in microseconds.  Returns true is sucessful,
	// or false if all hardware timers are already in use.
	bool begin(void (*funct)(), double microseconds) {
		return begin(funct, (float)microseconds);
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(unsigned int microseconds) {
		if (microseconds == 0 || microseconds > MAX_PERIOD) return;
		uint32_t cycles = (24000000 / 1000000) * microseconds - 1;
		if (cycles < 17) return;
		if (channel) channel->LDVAL = cycles;
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(int microseconds) {
		if (microseconds < 0) return;
		return update((unsigned int)microseconds);
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(unsigned long microseconds) {
		return update((unsigned int)microseconds);
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(long microseconds) {
		return update((int)microseconds);
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(float microseconds) {
		if (microseconds <= 0 || microseconds > MAX_PERIOD) return;
		uint32_t cycles = (float)(24000000 / 1000000) * microseconds - 0.5f;
		if (cycles < 17) return;
		if (channel) channel->LDVAL = cycles;
	}
	// Change the timer's interval.  The current interval is completed
	// as previously configured, and then the next interval begins with
	// with this new setting.
	void update(double microseconds) {
		return update((float)microseconds);
	}
	// Stop calling the function. The hardware timer resource becomes available
	// for use by other IntervalTimer instances.
	void end();
	// Set the interrupt priority level, controlling which other interrupts this
	// timer is allowed to interrupt. Lower numbers are higher priority, with 0
	// the highest and 255 the lowest. Most other interrupts default to 128. As
	// a general guideline, interrupt routines that run longer should be given
	// lower priority (higher numerical values).
	void priority(uint8_t n) {
		nvic_priority = n;
		if (channel) {
			int index = channel - IMXRT_PIT_CHANNELS;
			nvic_priorites[index] = nvic_priority;
			uint8_t top_priority = nvic_priorites[0];
			for (uint8_t i=1; i < (sizeof(nvic_priorites)/sizeof(nvic_priorites[0])); i++) {
				if (top_priority > nvic_priorites[i]) top_priority = nvic_priorites[i];
			}
			NVIC_SET_PRIORITY(IRQ_PIT, top_priority);
		}
	}
	operator IRQ_NUMBER_t() {
		if (channel) {
			return IRQ_PIT;
		}
		return (IRQ_NUMBER_t)NVIC_NUM_INTERRUPTS;
	}
private:
//#define IMXRT_PIT_CHANNELS              ((IMXRT_PIT_CHANNEL_t *)(&(IMXRT_PIT.offset100)))
	IMXRT_PIT_CHANNEL_t *channel = nullptr;
	uint8_t nvic_priority = 128;
	static uint8_t nvic_priorites[4];
	bool beginCycles(void (*funct)(), uint32_t cycles);

};


#ifdef __cplusplus
}
#endif

#endif
