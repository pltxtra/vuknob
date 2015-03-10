#define __SATAN_USES_FXP
//#define __SATAN_USES_FLOATS

#include "libtestbench_timer.c"
#include "europa4.c"
#include "libtestbench.c"

#define STEP_LENGTH 240
#define MAX_STEPS  18370
#define TEST_LENGTH (STEP_LENGTH * MAX_STEPS)

void *input_signal[TEST_LENGTH];
FTYPE output_signal[TEST_LENGTH];	

FTYPE a_signal[TEST_LENGTH];	
FTYPE b_signal[TEST_LENGTH];		

int main(int argc, char **argv) {
	struct mockery m;

	
	if(prepare_mockery(&m, STEP_LENGTH, "europa4")) {
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

	if(create_mock_intermediate_signal(
		   &m, "A",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   a_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	if(create_mock_intermediate_signal(
		   &m, "B",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   b_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));

	uint8_t on_event_A[8], off_event_A[8];
	MidiEvent *on_A = (MidiEvent *)on_event_A;
	MidiEvent *off_A = (MidiEvent *)off_event_A;
	on_A->length = 4; off_A->length = 4;

	on_A->data[0] = MIDI_NOTE_ON | 0;
	on_A->data[1] = 38;
	on_A->data[2] = 127;
	
	off_A->data[0] = MIDI_NOTE_OFF | 0;
	off_A->data[1] = 38;
	off_A->data[2] = 127;

	uint8_t on_event_B[8], off_event_B[8];
	MidiEvent *on_B = (MidiEvent *)on_event_B;
	MidiEvent *off_B = (MidiEvent *)off_event_B;
	on_B->length = 4; off_B->length = 4;

	on_B->data[0] = MIDI_NOTE_ON | 0;
	on_B->data[1] = 50;
	on_B->data[2] = 127;
	
	off_B->data[0] = MIDI_NOTE_OFF | 0;
	off_B->data[1] = 50;
	off_B->data[2] = 127;

	input_signal[10] = on_A;
	input_signal[8000] = off_A;
	input_signal[8003] = on_B;
	input_signal[60000] = off_B;
/*
	FTYPE *env2_attack = get_controller(&m, "Attack", "ENV-2");
	*env2_attack = ftoFTYPE(0.0f);

	int *vcf_mode = get_controller(&m, "Mode", "VCF");
	*vcf_mode = 1;
	FTYPE *cutoff = get_controller(&m, "Cutoff", "VCF");
	*cutoff = ftoFTYPE(0.2f);
	FTYPE *env = get_controller(&m, "Env", "VCF");
	*env = ftoFTYPE(1.0f);
	int *selector = get_controller(&m, "EnvSelector", "VCF");
	*selector = 0;

	FTYPE *release = get_controller(&m, "Release", "ENV-2");
	*release = ftoFTYPE(0.945);
	
	int *vco1_sin = get_controller(&m, "Sin", "VCO-1");
	*vco1_sin = 0;
	int *vco1_saw = get_controller(&m, "Saw", "VCO-1");
	*vco1_saw = 1;
	int *vco2_saw = get_controller(&m, "Saw", "VCO-2");
	*vco2_saw = 1;
	int *vco1_sawsmooth = get_controller(&m, "SawSmooth", "VCO-1");
	*vco1_sawsmooth = 0;
	int *vco2_sawsmooth = get_controller(&m, "SawSmooth", "VCO-2");
	*vco2_sawsmooth = 0;
	
	*/
	
	int *general_mode = get_controller(&m, "Mode", "General");
	*general_mode = 0;
	
	make_a_mockery(&m, MAX_STEPS);

//	write_wav(a_signal, b_signal, TEST_LENGTH, "/tmp/mocking.wav");
	write_wav(output_signal, output_signal, TEST_LENGTH, "/tmp/mocking.wav");

	printf("\n\n");
	printf("output: %p\n", output_signal);

	(void) plot_wave("/tmp/mocking.wav", 0, 1 * 44100);

	return 0;
}
