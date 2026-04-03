#include "daisy_petal.h"
#include "daisysp.h"
#include "delayline_reverse.h"
#include "terrarium.h"

using namespace terrarium;
using namespace daisy;
using namespace daisysp;

DaisyPetal hw;

Led led1, led2;

// ---------------------------- CONSTANTS ----------------------------------

#define kPreGain 1
#define kSampleRate 48000


// -------------------------- FUNCTION DECS -------------------------------

float ScaleNum(float num, float newMin, float newMax);
void ConditionalParameter(float  &oldVal, float  newVal, float &param);
void InitializeADC();

// ----------------------------- ENUMS ------------------------------------

enum EffectsChannels {
	delayMode,
	flangerMode,
	reverbMode
};

// -------------  Global variables to chage later on -----------------

// misc variables
bool bypass = false;
EffectsChannels effect = delayMode;

// knob variables to be stored
float k1 = 0;
float k2 = 0;
float k3 = 0; 
float k4 = 0;
float k5 = 0;


// delay variables
float gDelayTime = 1.0f; 
float gCurrentDelayTime = 1.0f;
float gDelayFeedback = 0.3f;
float gDelayDryWet = 0.5f;
bool revDelay = true;

// distortion variable
float gDistortion = 1.0f; // 1 == no distortion

// flanger variables
float gFlangerDepth = 0.5f;
float gFlangerFreq = 0.33f;
float gFlangerFeedback = 0.83f;
float gFlangerDryWet = 0.0f;

// bitcrusher variables
float gReverbTime = 0.6f;
float gReverbFreq = 500.0f;
float gReverbDryWet = 0.0f;

// filter variables
float gFilterFreq = 10000.0f;

// gain variables
float gPostGain = 1.0f;

//DelayLine::SetBuffer();

// -------------------- defining delay lines & chorus ------------------------

#define MAX_DELAY static_cast<size_t>(48000 * 0.75f)
//static DelayLine<float, MAX_DELAY> del;
//static DelayLineReverse<float, MAX_DELAY> delRev;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS del;
static DelayLineReverse<float, MAX_DELAY> DSY_SDRAM_BSS delRev;

//Flanger flanger;
Flanger flanger;
//static ReverbSc DSY_SDRAM_BSS verb;
ReverbSc verb;
MoogLadder filt;
Overdrive drive;
Metro met;
Oscillator lfo;


// --------------------------------------------------------------------

void ConditionalParameter(float  &oldVal, float  newVal, float &param)
{
    if(abs(oldVal - newVal) > 0.005f)
    {
        param = newVal;
		oldVal = newVal;
    }
}

float ScaleNum(float num, float newMin, float newMax)
{
	float oldMin = 0;
	float oldMax = 1;

	return (((newMax - newMin) * (num - oldMin) / (oldMax - oldMin)) + newMin);
}

void ProccessADC()
{
	// hw.ProcessAllControls();
	gDistortion = hw.knob[Terrarium::KNOB_5].Process();
	gPostGain = hw.knob[Terrarium::KNOB_4].Process();

	switch(effect)
	{
		case delayMode:
			ConditionalParameter(k1, hw.knob[Terrarium::KNOB_1].Process(), gDelayTime); // only maps when pot is changed
			ConditionalParameter(k2, hw.knob[Terrarium::KNOB_2].Process(), gDelayFeedback);
			ConditionalParameter(k5, hw.knob[Terrarium::KNOB_6].Process(), gDelayDryWet);
			ConditionalParameter(k3, hw.knob[Terrarium::KNOB_3].Process(), gFilterFreq);
			break;
		case flangerMode:
			ConditionalParameter(k1, hw.knob[Terrarium::KNOB_1].Process(), gFlangerFreq);
			ConditionalParameter(k2, hw.knob[Terrarium::KNOB_2].Process(), gFlangerDepth);
			ConditionalParameter(k3, hw.knob[Terrarium::KNOB_3].Process(), gFlangerFeedback);
			ConditionalParameter(k5, hw.knob[Terrarium::KNOB_6].Process(), gFlangerDryWet);
			break;
		case reverbMode:
			ConditionalParameter(k1, hw.knob[Terrarium::KNOB_1].Process(), gReverbTime);
			ConditionalParameter(k2, hw.knob[Terrarium::KNOB_2].Process(), gReverbFreq);
			ConditionalParameter(k5, hw.knob[Terrarium::KNOB_6].Process(), gReverbDryWet);
			break;
		default:
			System::ResetToBootloader();
			break;
	}
	// change delays and filter
	fonepole(gCurrentDelayTime, gDelayTime, .00007f);
	del.SetDelay(kSampleRate * ScaleNum(gCurrentDelayTime, 0.01f, 1.0f));
	delRev.SetDelay1(kSampleRate * gCurrentDelayTime);
	//delRev.SetDelay1(kSampleRate * ScaleNum(gCurrentDelayTime, 0.01f, 0.5f));
	filt.SetFreq(ScaleNum(gFilterFreq, 300.0f, 20000.0f));

	// change flanger
	flanger.SetFeedback(gFlangerFeedback);
	flanger.SetLfoDepth(gFlangerDepth);
	flanger.SetLfoFreq(gFlangerFreq);

	// change bitcrusher
	verb.SetFeedback((ScaleNum(gReverbTime, 0.6f, 0.999f)));
	//verb.SetFeedback(0.7f);
	verb.SetLpFreq((ScaleNum(gReverbFreq, 500.0f, 20000.0f)));
	//verb.SetLpFreq(10000.0f);

	// change distortion
	drive.SetDrive(ScaleNum(gDistortion, 0.1f, 1.0f));
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	float feedback, sig_out, delRev_out, del_out, fla_out, wetl, wetr;

	hw.ProcessAllControls();
	ProccessADC();
	led1.Update();
    led2.Update();

	// CHANGE MODE SWITCH
	if(hw.switches[Terrarium::FOOTSWITCH_2].RisingEdge())
	{
		switch(effect)
		{
			case delayMode:
				effect = flangerMode;
				break;
			case flangerMode:
				effect = reverbMode;
				break;
			case reverbMode:
				effect = delayMode;
				break;
			default:
				System::ResetToBootloader();
				break;
		}
	}

	// BYPASS SWITCH
	if(hw.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

	// REVERSE DELAY
	if(hw.switches[Terrarium::SWITCH_1].Pressed())
		revDelay = false;
	else
		revDelay = true;

	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = in[0][i] * kPreGain;

		if(!bypass)
		{
			// FLANGER PROCCESSING
			if(hw.switches[Terrarium::SWITCH_3].Pressed())
			{
				fla_out = flanger.Process(out[0][i]);
				out[0][i] = (fla_out * gFlangerDryWet) + (out[0][i] * (1.0f - gFlangerDryWet));
			}

			// DELAY PROCCESSING
			del_out = del.Read();
			delRev_out = delRev.ReadRev();

			// DELAY
			if(hw.switches[Terrarium::SWITCH_2].Pressed())
			{
				// checking if reverse switch is pressed
				if(revDelay)
				{
					sig_out = (delRev_out * gDelayDryWet) + (out[0][i] * (1.0f - gDelayDryWet));
					feedback = (delRev_out * gDelayFeedback) + out[0][i];
				}
				else
				{
					sig_out = (del_out * gDelayDryWet) + (out[0][i] * (1.0f - gDelayDryWet));
					feedback = (del_out * gDelayFeedback) + out[0][i];
				}
				del.Write(filt.Process(feedback)); 
				delRev.Write(filt.Process(feedback));
			}
			else
			{
				sig_out = out[0][i];
			}

			out[0][i] = drive.Process(sig_out);

			// BITCRUSHER PROCCESSING
			if(hw.switches[Terrarium::SWITCH_4].Pressed())
			{
				verb.Process(out[0][i], out[0][i], &wetl, &wetr);
				out[0][i] = (wetl * gReverbDryWet) + (out[0][i] * (1.0f - gReverbDryWet));
			}
		}
		out[0][i] *= gPostGain;
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.StartAdc();
	
	del.Init();
	delRev.Init();

	led1.Init(hw.seed.GetPin(Terrarium::LED_1),false);
    led2.Init(hw.seed.GetPin(Terrarium::LED_2),false);
	led1.Set(1);
    led1.Update();
	led2.Update();
	
	// Initialize chorus
	flanger.Init(kSampleRate);
	flanger.SetLfoFreq(gFlangerFreq);
	flanger.SetLfoDepth(gFlangerDepth);
	flanger.SetFeedback(gFlangerFeedback);

	// Initialize bitcrusher
	float sample_rate = hw.AudioSampleRate();
	verb.Init(sample_rate);
	verb.SetFeedback(gReverbTime);
	verb.SetLpFreq(gReverbFreq);

	// Initialize filter
	filt.Init(kSampleRate);
	filt.SetRes(0.0f);

	// Initialize flanger LFO for LED
	lfo.Init(kSampleRate);
	lfo.SetWaveform(Oscillator::WAVE_SQUARE);
	lfo.SetFreq(0.1f);
	
	

	hw.StartAudio(AudioCallback);

	while(1) {
		led2.Set(lfo.Process());
		switch(effect)
		{
			case delayMode:
				lfo.SetFreq(0.015f);
				break;
			case flangerMode:
				lfo.SetFreq(0.03f);
				break;
			case reverbMode:
				lfo.SetFreq(0.05f);
				break;
			default:
				System::ResetToBootloader();
				break;
		}
		led2.Update();
	}
}

 
