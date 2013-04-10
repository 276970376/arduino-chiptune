#include "oscillators.h"

volatile Oscillator osc[OSCILLATORS];
uint16_t noise = 0xACE1;
int envelopeUpdateCounter = 0;
int tickCounter = 0;  // tick rate 16 Khz

void initOscillators()
{
	memset((void*)osc, 0, sizeof(osc));
}

void updateEnvelopes();

uint8_t updateOscillators()
{
	// Galois LFSR, see http://en.wikipedia.org/wiki/Linear_feedback_shift_register
	noise = (noise >> 1) ^ (-(noise & 1) & 0xB400u);

	int16_t output = 0;

	for(int i=0; i<OSCILLATORS; i++)
	{
		// update oscillator phase
		osc[i].phase += osc[i].frequency;
		int16_t phase = osc[i].phase >> 8; // [0,255]

		int16_t value = 0;  // [-32,31]

		switch(osc[i].waveform)
		{
		case TRIANGLE: 
			if(phase < 128)
				value = (phase>>1) - 32;
			else
				value = ((255 - phase)>>1) - 31;
			break;

		case PULSE:
			value = (phase < osc[i].pulseWidth ? -32 : 31);
			break;

		case SAWTOOTH:
			value = (phase>>2) - 32;
			break;

		case NOISE:
			if(phase > 64)
			{
				osc[i].noise = (int)(noise >> 10) - 32;
				osc[i].phase -= 16384;
			}
			value = osc[i].noise;
			break;
		}
		   
		int16_t amp = osc[i].amp>>8;  // [0,127]
		output += value * amp;  // [-4096,4095]
	}

	// update envelopes every 16th cycle
	if(envelopeUpdateCounter == 0)
	{
		updateEnvelopes();
		envelopeUpdateCounter = 16;
	}
	envelopeUpdateCounter--;

#if OSCILLATORS == 1
	return (output>>5) + 128;
#elif OSCILLATORS == 2
	return (output>>6) + 128;
#elif OSCILLATORS == 3
	// (x*10)>>5 is roughly same as x/3 but much faster
	return ((output>>5)*10>>5) + 128;
#elif OSCILLATORS == 4
	return (output>>7) + 128;
#else
	#error "Invalid number of oscillators"
#endif
}

inline void updateEnvelopes()
{
	for(int i=0; i<OSCILLATORS; i++)
	{
		if(osc[i].ctrl == 1)
		{
			// attack: fade to peak amplitude, start decay when peak amplitude reached
			uint16_t amp = osc[i].amp;
			amp += ((uint16_t)osc[i].attack)<<2;
			if(amp >= 0x7fff)
			{
				amp = 0x7fff;
				osc[i].ctrl = 2;
			}
			osc[i].amp = amp;
		}
		else if(osc[i].ctrl == 2)
		{
			// decay: fade to sustain amplitude
			int16_t amp = osc[i].amp;
			amp = max(amp - ((int16_t)osc[i].decay<<2), osc[i].sustain<<8);
			osc[i].amp = amp;
		}
		else
		{
			// release: fade to zero amplitude
			int16_t amp = osc[i].amp;
			amp = max(amp - osc[i].release, 0);
			osc[i].amp = amp;
		}
	}
}

void resetOscillators()
{
	for(int i = 0; i < OSCILLATORS; i++)
	{
		osc[i].phase = 0;
		osc[i].frequency = 0;
		osc[i].ctrl = 0;
	}
}
