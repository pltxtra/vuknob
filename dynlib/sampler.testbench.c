//#define __SATAN_USES_FXP
#define __SATAN_USES_FLOATS

#ifdef __SATAN_USES_FXP
#define OUTPUT_FNAME "/tmp/mocking_fxp.wav"
#else
#define OUTPUT_FNAME "/tmp/mocking_float.wav"
#endif

#include "sampler.c"
#include "libtestbench.c"

#define TEST_LENGTH 44100 * 4

void *input_signal[TEST_LENGTH];
FTYPE output_signal[TEST_LENGTH];	

FTYPE a_signal[TEST_LENGTH];	
FTYPE b_signal[TEST_LENGTH];		

int main(int argc, char **argv) {
	struct mockery m;

	
	if(prepare_mockery(&m, TEST_LENGTH, "sampler")) {
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

	{
		int rd;
		int16_t riffdata[TEST_LENGTH * 2];
		
		RIFF_WAVE_FILE_t rwf;
		
		RIFF_open_file(&rwf, "../Samples/Drums/Clap.wav");

		rd = RIFF_read_data(&rwf, riffdata, TEST_LENGTH);

		convert_pcm_to_static_signal(
			riffdata,
			rwf.channels, rwf.samples);

		RIFF_close_file(&rwf);
		printf("   read: %d\n", rd);
		printf("data[0] = %d\n", riffdata[0]);
		printf("data[1] = %d\n", riffdata[1]);
		printf("data[2] = %d\n", riffdata[2]);
		printf("data[3] = %d\n", riffdata[3]);
	}
	
	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));

	uint8_t on_event[8], off_event[8];
	MidiEvent *on = (MidiEvent *)on_event;
	MidiEvent *off = (MidiEvent *)off_event;
	on->length = 4; off->length = 4;

	on->data[0] = MIDI_NOTE_ON | 0;
	on->data[1] = 50;
	on->data[2] = 127;
	
	off->data[0] = MIDI_NOTE_OFF | 0;
	off->data[1] = 50;
	off->data[2] = 127;

	input_signal[10] = on;
	input_signal[10000] = off;
	
	make_a_mockery(&m);

//	write_wav(a_signal, b_signal, TEST_LENGTH, OUTPUT_FNAME);
//	write_wav(a_signal, output_signal, TEST_LENGTH, OUTPUT_FNAME);
	write_wav(output_signal, output_signal, TEST_LENGTH, OUTPUT_FNAME);

	printf("\n\n");
	printf("output: %p\n", output_signal);

	return 0;
}
