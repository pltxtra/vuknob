//#define __SATAN_USES_FXP
#define __SATAN_USES_FLOATS

#include "libtestbench_timer.c"
#include "silverbox.c"
#include "libtestbench.c"

#define STEP_LENGTH 240
#define MAX_STEPS  1837
#define TEST_LENGTH (STEP_LENGTH * MAX_STEPS)

void *input_signal[TEST_LENGTH];
FTYPE output_signal[TEST_LENGTH];	

int main(int argc, char **argv) {
	struct mockery m;

	
	if(prepare_mockery(&m, STEP_LENGTH, "silverbox")) {
		printf("Failed to initiate mockery of module.\n");
		exit(1);
	}

	if(create_mock_input_signal(
		   &m, "midi",
		   _MIDI,
		   1,
		   _PTR,
		   44100,
		   TEST_LENGTH,
		   input_signal
		   )) {
		printf("Failed to create mock input signal.\n");
		exit(2);
	}

	if(create_mock_output_signal(
		   &m, "Mono",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   output_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	FTYPE *volume = get_controller(&m, "volume", "general");
	*volume = ftoFTYPE(0.5f);

	FTYPE *resonance = get_controller(&m, "resonance", "general");
	*resonance = ftoFTYPE(1.0f);

	FTYPE *cutoff = get_controller(&m, "cutoff", "general");
	*cutoff = ftoFTYPE(1.0f);

	int *wave = get_controller(&m, "wave", "general");
	*wave = 0;

	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));

	uint8_t on_event[8], off_event[8];
	MidiEvent *on = (MidiEvent *)on_event;
	MidiEvent *off = (MidiEvent *)off_event;
	on->length = 4; off->length = 4;

	on->data[0] = MIDI_NOTE_ON | 0;
	on->data[1] = 28;
	on->data[2] = 127;
	
	off->data[0] = MIDI_NOTE_OFF | 0;
	off->data[1] = 28;
	off->data[2] = 127;

	input_signal[10] = on;
	input_signal[10000] = off;
	
	make_a_mockery(&m, MAX_STEPS);

	write_wav(output_signal, output_signal, TEST_LENGTH, "/tmp/mocking.wav");

	(void) plot_wave("/tmp/mocking.wav", 0, 4 * 4410);

	return 0;
}
