#define __SATAN_USES_FXP
//#define __SATAN_USES_FLOATS

#include "rewerb.c"
#include "libtestbench.c"

#define TEST_LENGTH 44100 * 10

int main(int argc, char **argv) {
	struct mockery m;

	FTYPE input_signal[TEST_LENGTH];
	FTYPE output_signal[TEST_LENGTH];	
	
	if(prepare_mockery(&m, TEST_LENGTH, "rewerb")) {
		printf("Failed to initiate mockery of module rewerb.\n");
		exit(1);
	}

	if(create_mock_input_signal(
		   &m, "Mono",
		   _0D,
		   1,
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
		   44100,
		   TEST_LENGTH,
		   output_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));
/*
	{
		int k;
		for(k = 0; k < 1; k++) {
			input_signal[k] = ftoFTYPE(0.5f);
		}
	}
*/
	{
		int rd;
		int16_t riffdata[TEST_LENGTH * 2];
		
		RIFF_WAVE_FILE_t rwf;
		
		RIFF_open_file(&rwf, "../Samples/Drums/Clap.wav");

		rd = RIFF_read_data(&rwf, riffdata, TEST_LENGTH);

		convert_pcm_to_mock_signal(
			input_signal, 1, TEST_LENGTH,

			riffdata,
			rwf.channels, rwf.samples);

		RIFF_close_file(&rwf);
		printf("   read: %d\n", rd);
		printf("data[0] = %d\n", riffdata[0]);
		printf("data[1] = %d\n", riffdata[1]);
		printf("data[2] = %d\n", riffdata[2]);
		printf("data[3] = %d\n", riffdata[3]);
	}

	float *feedback = get_controller(&m, "FeedbackGain", "");
	*feedback = 0.22f;
	
	float *drylevel = get_controller(&m, "DryLevel", "");
	*drylevel = 0.0f;
	
	int *type = get_controller(&m, "Type", "");
	*type = 1;
	
	make_a_mockery(&m);

	write_wav(output_signal, input_signal, TEST_LENGTH, "/tmp/mocking.wav");

	printf("\n\n");
	printf("input: %p\n", input_signal);
	printf("output: %p\n", output_signal);
}
