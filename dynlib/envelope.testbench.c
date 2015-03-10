#define __SATAN_USES_FXP
//#define __SATAN_USES_FLOATS

#include "libtestbench_timer.c"
#include "envelope.c"
#include "libtestbench.c"

#define STEP_LENGTH 44100
#define MAX_STEPS  40
#define TEST_LENGTH (STEP_LENGTH * MAX_STEPS)

FTYPE output_signal[TEST_LENGTH];	

int main(int argc, char **argv) {
	struct mockery m;

	
	if(prepare_mockery(&m, STEP_LENGTH, "envelope")) {
		printf("Failed to initiate mockery of module.\n");
		exit(1);
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

	memset(output_signal, 0, sizeof(output_signal));

	make_a_mockery(&m, MAX_STEPS);

	write_wav(output_signal, output_signal, TEST_LENGTH, "/tmp/mocking.wav");

	printf("\n\n");
	printf("output: %p\n", output_signal);

	(void) plot_wave("/tmp/mocking.wav", 0, 4 * 44100);
	
	return 0;
}
